// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

/// @file libhpx/network/network.c
/// @brief Manages the HPX network.
///
/// This file deals with the complexities of the HPX network interface,
/// shielding it from the details of the underlying transport interface.
#include <assert.h>
#include <stdlib.h>
#include <pthread.h>

#include <libsync/sync.h>
#include <libsync/queues.h>
#include <libsync/spscq.h>
#include <libsync/locks.h>

#include "libhpx/boot.h"
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/network.h"
#include "libhpx/parcel.h"
#include "libhpx/stats.h"
#include "libhpx/system.h"
#include "libhpx/transport.h"
#include "libhpx/routing.h"


//#define _QUEUE(pre, post) pre##spscq##post
//#define _QUEUE(pre, post) pre##ms_queue##post
#define _QUEUE(pre, post) pre##two_lock_queue##post
#define _QUEUE_T _QUEUE(, _t)
#define _QUEUE_INIT _QUEUE(sync_, _init)
#define _QUEUE_FINI _QUEUE(sync_, _fini)
#define _QUEUE_ENQUEUE _QUEUE(sync_, _enqueue)
#define _QUEUE_DEQUEUE _QUEUE(sync_, _dequeue)
#define _QUEUE_NODE _QUEUE(,_node_t)


/// The network class data.
struct _network {
  struct network             vtable;
  volatile int                flush;
  struct transport_class *transport;
  pthread_t                progress;
  int                           nrx;

  // make sure the rest of this structure is cacheline aligned
  const char _paddinga[HPX_CACHELINE_SIZE - ((sizeof(struct network) +
                                              sizeof(int) +
                                              sizeof(struct transport_class*) +
                                              sizeof(pthread_t) +
                                              sizeof(int)) %
                                             HPX_CACHELINE_SIZE)];

  _QUEUE_T                 tx;                  // half duplex port for send
  struct clh_lock      rxlock;
  const char _paddingb[HPX_CACHELINE_SIZE - sizeof(struct clh_lock)];
  hpx_parcel_t * volatile *volatile prx;
  const char _paddingc[HPX_CACHELINE_SIZE - sizeof(hpx_parcel_t*)];
  struct _rxnode {
    struct clh_node     *node;
    struct clh_node     *poll;
    hpx_parcel_t * volatile stack;
    int           handshaking;
    const char _padding[HPX_CACHELINE_SIZE - (2 * sizeof(struct clh_node*) +
                                              sizeof(hpx_parcel_t *) +
                                              sizeof(int))];
  } rxnodes[];
} HPX_ALIGNED(HPX_CACHELINE_SIZE);


static void *_progress(void *o) {
  struct _network *network = o;

  // we have to join the GAS so that we can allocate parcels in here.
  int e = here->gas->join();
  if (e) {
    dbg_error("network failed to join the global address space.\n");
  }

  while (true) {
    pthread_testcancel();
    profile_ctr(thread_get_stats()->progress++);
    transport_progress(network->transport, TRANSPORT_POLL);
    pthread_yield();
  }
  return NULL;
}


static void _delete(struct network *o) {
  if (!o)
    return;

  struct _network *network = (struct _network*)o;

  hpx_parcel_t *p = NULL;

  while ((p = _QUEUE_DEQUEUE(&network->tx))) {
    hpx_parcel_release(p);
  }
  _QUEUE_FINI(&network->tx);

  // for (int i = 0, e = network->nrx; i < e; ++i) {
  //   sync_clh_node_delete(network->rxnodes[i].node);
  //   //assert(network->rxnodes[i].poll == NULL);
  // }
  // sync_clh_lock_fini(&network->rxlock);

  free(network);
}


static int _startup(struct network *o) {
  struct _network *network = (struct _network*)o;
  int e = pthread_create(&network->progress, NULL, _progress, network);
  if (e) {
    dbg_error("failed to start network progress.\n");
    return LIBHPX_ERROR;
  }
  else {
    dbg_log("started network progress.\n");
    return LIBHPX_OK;
  }
}


static void _shutdown(struct network *o) {
  struct _network *network = (struct _network*)o;
  int e = pthread_cancel(network->progress);
  if (e) {
    dbg_error("could not cancel the network progress thread.\n");
  }

  e = pthread_join(network->progress, NULL);
  if (e) {
    dbg_error("could not join the network progress thread.\n");
  }
  else {
    dbg_log("shutdown network progress.\n");
  }

  int flush = sync_load(&network->flush, SYNC_ACQUIRE);
  if (flush) {
    transport_progress(network->transport, TRANSPORT_FLUSH);
  }
  else {
    transport_progress(network->transport, TRANSPORT_CANCEL);
  }
}


static void _barrier(struct network *o) {
  struct _network *network = (struct _network*)o;
  transport_barrier(network->transport);
}


void network_tx_enqueue(struct network *o, hpx_parcel_t *p) {
  struct _network *network = (struct _network*)o;
  _QUEUE_ENQUEUE(&network->tx, p);
}


hpx_parcel_t *network_tx_dequeue(struct network *o) {
  struct _network *network = (struct _network*)o;
  return _QUEUE_DEQUEUE(&network->tx);
}


void network_rx_enqueue(struct network *o, hpx_parcel_t *p) {
  assert(false);
}


hpx_parcel_t *network_rx_dequeue(struct network *o, int nrx) {
  struct _network *network = (struct _network*)o;
  struct _rxnode *rxn = &network->rxnodes[nrx];
  struct clh_node *node = rxn->node;
  struct clh_node *poll = rxn->poll;

  // publish my intent to process the network
  if (!poll) {
    poll = sync_clh_lock_start_acquire(&network->rxlock, node);
    rxn->poll = poll;
  }

  // wait until I get control of the network
  if (sync_clh_node_must_wait(poll)) {
    return NULL;
  }

  // first time through publish my parcel stack pointer
  if (!rxn->handshaking) {
    sync_store(&network->prx, &rxn->stack, SYNC_RELEASE);
    rxn->handshaking = 1;
  }

  // see if the network thread noticed me yet
  hpx_parcel_t *p = sync_load(&rxn->stack, SYNC_ACQUIRE);
  if (!p) {
    return NULL;
  }

  // I got some work, release the lock, and reset my rxnode
  rxn->node = sync_clh_lock_release(&network->rxlock, node);
  rxn->poll = NULL;
  rxn->stack = NULL;
  rxn->handshaking = 0;
  return p;
}

int network_try_notify_rx(struct network *o, hpx_parcel_t *p) {
  struct _network *network = (struct _network*)o;
  // if someone has published a rendevous location, pass along the parcel
  // stack
  hpx_parcel_t * volatile *prx = sync_load(&network->prx, SYNC_ACQUIRE);
  if (!prx) {
    return 0;
  }
  dbg_log_trans("sent a parcel.\n");
  sync_store(&network->prx, NULL, SYNC_RELEASE);
  sync_store(prx, p, SYNC_RELEASE);
  return 1;
}

void network_flush_on_shutdown(struct network *o) {
  struct _network *network = (struct _network*)o;
  sync_store(&network->flush, 1, SYNC_RELEASE);
}

struct network *network_new(int nrx) {
  struct _network *n = NULL;
  int e = posix_memalign((void**)&n, HPX_CACHELINE_SIZE, sizeof(*n)
                         + nrx * sizeof(n->rxnodes[0]));
  if (e) {
    dbg_error("failed to allocate a network.\n");
    return NULL;
  }

  assert((uintptr_t)&n->tx % HPX_CACHELINE_SIZE == 0);
  assert((uintptr_t)&n->rxlock % HPX_CACHELINE_SIZE == 0);
  assert((uintptr_t)&n->rxnodes % HPX_CACHELINE_SIZE == 0);

  n->vtable.delete = _delete;
  n->vtable.startup = _startup;
  n->vtable.shutdown = _shutdown;
  n->vtable.barrier = _barrier;

  n->transport = here->transport;
  sync_store(&n->flush, 0, SYNC_RELEASE);
  n->nrx = nrx;

  _QUEUE_INIT(&n->tx, 0);
  sync_clh_lock_init(&n->rxlock);
  sync_store(&n->prx, NULL, SYNC_RELEASE);
  for (int i = 0; i < nrx; ++i) {
    n->rxnodes[i].node = sync_clh_node_new();
    n->rxnodes[i].poll = NULL;
    n->rxnodes[i].stack = NULL;
    n->rxnodes[i].handshaking = 0;
  }

  return &n->vtable;
}
