/*
  ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Functions
  hpx_parcelhandler.c

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).

  Authors:
  Benjamin D. Martin <benjmart [at] indiana.edu>
  ====================================================================
*/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include "parcelhandler.h"                      /* public interface */
#include "hpx/globals.h"                        /* __hpx_network_ops */
#include "hpx/lco.h"                            /* struct hpx_future */
#include "hpx/thread/ctx.h"                     /* struct hpx_context */
#include "debug.h"                              /* dbg_ routines */
#include "hashstr.h"                            /* hashstr() */
#include "network.h"                            /* struct network_status */
#include "parcel.h"                             /* struct hpx_parcel */
#include "parcelqueue.h"                        /* struct parcelqueue */
#include "request_buffer.h"                     /* struct request_buffer */
#include "request_list.h"                       /* struct request_list */
#include "serialization.h"                      /* struct header */

// #define HPX_PARCELHANDLER_GET_THRESHOLD 0 // force all payloads to be sent
// via put/get 
/* At present, the threshold is used with the payload_size. BUT doesn't it make
// more sense to compare against total parcel size? The action name factors in
// then... */ 
#define HPX_PARCELHANDLER_GET_THRESHOLD SIZE_MAX

/* TODO: make configurable (and find a good, sane size) */
static const size_t REQUEST_BUFFER_SIZE     = 2048;
/* TODO: make configurable (and use a real size) */ 
static const size_t RECV_BUFFER_SIZE        = 1024*1024*16; 

/** Typedefs used in this source file @{ */
typedef struct hpx_context   context_t;
typedef struct hpx_thread     thread_t;
typedef struct hpx_future     future_t;
typedef struct hpx_parcel     parcel_t;
typedef struct header         header_t;
typedef struct parcelhandler handler_t;
typedef struct request_list  reqlist_t;
/** @} */

/**
 * The send queue for this locality.
 */
struct parcelqueue *__hpx_send_queue = NULL;

/**
 * The parcel handler structure.
 *
 * This is what is returned by parcelhandler_create().
 */
struct parcelhandler {
  context_t   *ctx;
  thread_t *thread;  
  future_t   *quit;                       /*!< signals parcel handler to quit */
  future_t    *fut;
};

/**
 * Take a header from the network and create a local thread to process it.
 *
 * @param[in] header - the header to process
 *
 * @returns HPX_SUCCESS or an error code
 */
static hpx_error_t
complete(header_t* header, bool send)
{
  dbg_assert_precondition(header);

  /* just free the header on a send completion */
  if (send) {
    __hpx_network_ops->unpin(header, header->size);
    hpx_free(header);
    return HPX_SUCCESS;
  }

  dbg_printf("%d: Received %zd bytes to buffer at %p "
             "with parcel_id=%u action=%" HPX_PRIu_hpx_action_t "\n",
             hpx_get_rank(), header->size, (void*)header,
             header->parcel_id, header->action);

  /* deserialize and free the header */
  parcel_t* parcel = deserialize(header);
  __hpx_network_ops->unpin(header, header->size);
  hpx_free(header);

  if (!parcel) {
    dbg_printf("Could not complete a recv or get request.\n");
    return HPX_ERROR;
  }
  
  hpx_action_invoke(parcel->action, parcel->payload, NULL);
  return HPX_SUCCESS;
}

typedef int(*test_function_t)(network_request_t*, int*, network_status_t*);

static int
complete_requests(reqlist_t* list, test_function_t test, bool send)
{
  int count = 0;
  request_list_begin(list);
  network_request_t* req;                       /* loop iterator */
  int flag;                                     /* used in test */
  while ((req = request_list_curr(list)) != NULL) {
    dbg_check_success(test(req, &flag, NULL));
    if (flag == 1) {
      header_t* header = request_list_curr_parcel(list);
      request_list_del(list);
      dbg_check_success(complete(header, send));
      ++count;
    } 
    request_list_next(list);
  }
  return count;
}

static void
parcelhandler_main(struct parcelhandler *args)
{
  int success;

  hpx_future_t* quit = args->quit;

  reqlist_t send_requests;
  reqlist_t recv_requests;
  request_list_init(&send_requests);
  request_list_init(&recv_requests);

  header_t* header;
  size_t i;
  int completions;

  network_request_t* req;

  size_t probe_successes, recv_successes, send_successes;
  int initiated_something, completed_something;

  int dst_rank; /* raw network address as opposed to internal hpx address */
  size_t size;

  int outstanding_recvs;
  int outstanding_sends;

  void* recv_buffer;
  size_t recv_size;
  int * retval;
  int flag;
  network_status_t status;

  outstanding_recvs = 0;
  outstanding_sends = 0;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;

  i = 0;

  if (HPX_DEBUG) {
    probe_successes= 0;
    recv_successes = 0;
    send_successes = 0;
    initiated_something = 0;
    completed_something = 0;
  }

  while (1) {
    
    /* ==================================
       Phase 1: Deal with sends 
       ==================================
       
       (1) cleanup outstanding sends/puts
       (2) check __hpx_send_queue
       + call network ops to send
    */

    /* cleanup outstanding sends/puts */
    if (outstanding_sends > 0) {
      completions = complete_requests(&send_requests, __hpx_network_ops->sendrecv_test, true);
      outstanding_sends -= completions;
      if (HPX_DEBUG && (completions > 0)) {
        completed_something = 1;
        send_successes++;
      }
    } /* if (outstanding_sends > 0) */
    
    /* check send queue */
    header = parcelqueue_trypop(__hpx_send_queue);
    if (header != NULL) {
      if (HPX_DEBUG) {
        initiated_something = 1;
      }
      if (header == NULL) {
        /* TODO: signal error to somewhere else! */
      }
      dst_rank = header->dest.locality.rank;
      //      printf("Sending %zd bytes from buffer at %tx\n", size, (ptrdiff_t)header);
      dbg_printf("%d: Sending %zd bytes from buffer at %p with "
                 "parcel_id=%u action=%" HPX_PRIu_hpx_action_t "\n",
                 hpx_get_rank(), header->size, (void*)header, header->parcel_id,
                 header->action);
      req = request_list_append(&send_requests, header);
      //      printf("Sending %zd bytes from buffer at %tx\n", size, (ptrdiff_t)header);
      dbg_printf("%d: Sending with request at %p from buffer at %p\n",
                 hpx_get_rank(), (void*)req, (void*)header); 
      __hpx_network_ops->send(dst_rank, 
                              header,
                              size,
                              req);
      outstanding_sends++;
    }

    /* ==================================
       Phase 2: Deal with remote parcels 
       ==================================
    */
    if (outstanding_recvs > 0) {
      completions = complete_requests(&recv_requests, __hpx_network_ops->sendrecv_test, false);
      outstanding_recvs -= completions;
      if (HPX_DEBUG && (completions > 0)) {
        completed_something = 1;
        recv_successes += completions;
      }
    }
  
    /* Now check for new receives */
    __hpx_network_ops->probe(NETWORK_ANY_SOURCE, &flag, &status);
    if (flag > 0) { /* there is a message to receive */
      if (HPX_DEBUG) {
        initiated_something = 1;
        probe_successes++;
      }
    
#if HAVE_PHOTON
      recv_size = (size_t)status.photon.size;
#else
      recv_size = status.count;
      // recv_size = RECV_BUFFER_SIZE;
#endif
      success = hpx_alloc_align((void**)&recv_buffer, 64, recv_size);
      if (success != 0 || recv_buffer == NULL) {
        __hpx_errno = HPX_ERROR_NOMEM;
        *retval = HPX_ERROR_NOMEM;
        goto error;
      } 
      __hpx_network_ops->pin(recv_buffer, recv_size);
      dbg_printf("%d: Receiving %zd bytes to buffer at %tx\n", hpx_get_rank(),
                 recv_size, (ptrdiff_t)recv_buffer); 
      req = request_list_append(&recv_requests, recv_buffer);
      __hpx_network_ops->recv(status.source, recv_buffer, recv_size, req);
      outstanding_recvs++;
    }
  
  
    if (HPX_DEBUG) {
      if (initiated_something != 0 || completed_something != 0) {
        dbg_printf("rank %d: initiated: %d\tcompleted "
                   "%d\tprobes=%d\trecvs=%d\tsend=%d\n", hpx_get_rank(),
                   initiated_something, 
                   completed_something, (int)probe_successes, (int)recv_successes, 
                   (int)send_successes); 
        initiated_something = 0;
        completed_something = 0;
      }
    }
  
    if (hpx_lco_future_isset(quit) == true)
      break;
    /* If we don't yield occasionally, any thread that get scheduled to this core will get stuck. */
    i++;
    if (i % 1000 == 0)
      hpx_thread_yield();    
  }
  
  dbg_printf("%d: Handler done after iter %d\n", hpx_get_rank(), (int)i);

error:  
  hpx_thread_exit((void*)retval);
}

struct parcelhandler *
parcelhandler_create(struct hpx_context *ctx)
{
  int ret = HPX_ERROR;
  struct parcelhandler *ph = NULL;

  /* create and initialize send queue */
  ret = parcelqueue_create(&__hpx_send_queue);
  if (ret != 0) {
    __hpx_errno = HPX_ERROR;
    goto error;
  }

  /* create thread */
  ph = hpx_alloc(sizeof(*ph));
  ph->ctx = ctx;
  ph->quit = hpx_alloc(sizeof(hpx_future_t));
  hpx_lco_future_init(ph->quit);
  hpx_error_t e = hpx_thread_create(ph->ctx,
                                    HPX_THREAD_OPT_SERVICE_COREGLOBAL,
                                    (void(*)(void*))parcelhandler_main,
                                    (void*)ph,
                                    &ph->fut,
                                    &ph->thread);
  if (e == HPX_ERROR)
    dbg_print_error(e, "Failed to start the parcel handler core service");

error:
  return ph;
}

void
parcelhandler_destroy(struct parcelhandler *ph)
{
  if (!ph)
    return;
  
  network_barrier();

  hpx_lco_future_set_state(ph->quit);
  hpx_thread_wait(ph->fut);

  hpx_lco_future_destroy(ph->quit);
  hpx_free(ph->quit);
  hpx_free(ph);
  parcelqueue_destroy(&__hpx_send_queue);

  return;
}
