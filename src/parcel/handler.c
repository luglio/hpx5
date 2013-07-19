/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Parcel Handler Functions
  hpx_parchandler.c

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

#include <stdio.h>
#include <string.h>
#include "hpx_config.h"
#include "hpx_ctx.h"
#include "hpx_error.h"
#include "hpx_network.h"
#include "hpx_parchandler.h"
#include "hpx_thread.h"

#if USE_PHOTON
#include <photon.h>
#endif

void *_hpx_parchandler_main_test(void) {
  int PING_TAG=0;
  int PONG_TAG=1;
  printf("Alive\n");
  fflush(NULL);
  int size;
  int rank;
  int next, prev;
  int success;
  int flag;
  int type;
  MPI_Status stat;

  size = hpx_network_size();
  rank = hpx_network_rank();

  //hpx_network_init(&argc,&argv);

  char* hostname = (char*)malloc(MPI_MAX_PROCESSOR_NAME*sizeof(char));
  int hostname_len;
  MPI_Get_processor_name(hostname, &hostname_len);
  printf("Running on %s\n", hostname);
  fflush(NULL);

  int buffer_size = 1024*1024;
  char* send_buffer = NULL;
  char* recv_buffer = NULL;
  char* copy_buffer = NULL;

  uint32_t send_request;
  uint32_t recv_request;

  send_buffer = (char*)malloc(buffer_size*sizeof(char));
  recv_buffer = (char*)malloc(buffer_size*sizeof(char));
  copy_buffer = (char*)malloc(buffer_size*sizeof(char));
  hpx_network_register_buffer(send_buffer, buffer_size);
  hpx_network_register_buffer(recv_buffer, buffer_size);

  if (rank == 0) {
    strcpy(send_buffer, "Message from proc 0"); 
    printf("send started on rank %d\n", rank);
    fflush(NULL);
    hpx_network_send_start(1, PING_TAG, send_buffer, buffer_size, 0, &send_request);
    printf("send done on rank %d\n", rank);
    fflush(NULL);
    hpx_network_wait(send_request);
    printf("send finished on rank %d\n", rank);
    fflush(NULL);

    sleep(5);

    printf("recv started on rank %d\n", rank);
    fflush(NULL);
    hpx_network_recv_start(1, PONG_TAG, recv_buffer, buffer_size, 0, &recv_request);
    printf("recv done on rank %d\n", rank);
    fflush(NULL);
    hpx_network_wait(recv_request);
    printf("recv finished on rank %d\n", rank);
    fflush(NULL);

    printf("Message from proc 1: %s\n", recv_buffer);
    fflush(NULL);
  }
  else if (rank == 1) {
    printf("recv started on rank %d\n", rank);
    fflush(NULL);
    hpx_network_recv_start(0, PING_TAG, recv_buffer, buffer_size, 0, &recv_request);
    printf("recv done on rank %d\n", rank);
    fflush(NULL);
    hpx_network_wait(recv_request);
    printf("recv finished on rank %d\n", rank);
    fflush(NULL);

    int str_length;
    strcpy(copy_buffer, "Received from proc 0 message: '");
    str_length = strlen(copy_buffer);
    strcpy(&copy_buffer[str_length], recv_buffer);
    str_length = strlen(copy_buffer);
    strcpy(&copy_buffer[str_length], "'");
    strcpy(send_buffer, copy_buffer);

    sleep(5);

    printf("send started on rank %d\n", rank);
    fflush(NULL);
    hpx_network_send_start(0, PONG_TAG, send_buffer, buffer_size, 0, &send_request);
    printf("send done on rank %d\n", rank);
    fflush(NULL);
    hpx_network_wait(send_request);
    printf("send finished on rank %d\n", rank);
    fflush(NULL);
  }
  printf("Data transmission finished on rank %d\n", rank);
  fflush(NULL);
  
  printf("Unregistering buffers on rank %d\n", rank);
  fflush(NULL);
  hpx_network_unregister_buffer(send_buffer, buffer_size);
  hpx_network_unregister_buffer(recv_buffer, buffer_size);

  //  printf("hpx finalize on rank %d\n", rank);
  //  fflush(NULL);
  //  hpx_network_finalize();

  free(send_buffer);
  free(recv_buffer);
  free(copy_buffer);

  free(hostname);

  printf("SUCCESS\n", rank);
  fflush(NULL);


  while (0) {
    /* TODO: wait for signal to exit... */
  }

  int * retval;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;
  hpx_thread_exit((void*)retval);

  return NULL;
}

void * _hpx_parchandler_main_pingpong(void) {
  int PING_TAG=0;
  int PONG_TAG=1;
  fflush(NULL);
  int size;
  int rank;
  int next, prev;
  int success;
  int flag;
  int type;
  MPI_Status stat;
  int i;
  int iter_limit=10;//1000;
  int buffer_size = 1024*1024;
  char* send_buffer = NULL;
  char* recv_buffer = NULL;
  char* copy_buffer = NULL;
  uint32_t send_request;
  uint32_t recv_request;

  struct timespec begin_ts;
  struct timespec end_ts;
  unsigned long elapsed;
  double avg_oneway_latency;

  MPI_Comm_size(MPI_COMM_WORLD, &size);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  // use MPI ==================================================================================================================
  send_buffer = (char*)malloc(buffer_size*sizeof(char));
  recv_buffer = (char*)malloc(buffer_size*sizeof(char));
  copy_buffer = (char*)malloc(buffer_size*sizeof(char));

  clock_gettime(CLOCK_MONOTONIC, &begin_ts);

  if (rank == 0) {
    for (i = 0; i < iter_limit; i++) {
      snprintf(send_buffer, 50, "Message %d from proc 0", i);
      MPI_Send(send_buffer, buffer_size, MPI_CHAR, 1, PING_TAG, MPI_COMM_WORLD);
      
      MPI_Recv(recv_buffer, buffer_size, MPI_CHAR, 1, PONG_TAG, MPI_COMM_WORLD, &stat);
      // printf("Message from proc 1: <<%s>>\n", recv_buffer);
    }
  }
  else if (rank == 1) {
    strcpy(send_buffer, "Message from proc 1");

    for (i = 0; i < iter_limit; i++) {
      MPI_Recv(recv_buffer, buffer_size, MPI_CHAR, 0, PING_TAG, MPI_COMM_WORLD, &stat);

      int str_length;
      snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", i);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], recv_buffer);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], "'");
      strcpy(send_buffer, copy_buffer);

      MPI_Send(send_buffer, buffer_size, MPI_CHAR, 0, PONG_TAG, MPI_COMM_WORLD);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);

  avg_oneway_latency = elapsed/((double)(iter_limit*2));

  printf("average oneway latency (MPI):   %f ms\n", avg_oneway_latency/1000000.0);

  free(send_buffer);
  free(recv_buffer);

  // end of use MPI part ===========================================================================================

#if USE_PHOTON
  send_buffer = (char*)malloc(buffer_size*sizeof(char));
  recv_buffer = (char*)malloc(buffer_size*sizeof(char));
  copy_buffer = (char*)malloc(buffer_size*sizeof(char));

  hpx_network_register_buffer(send_buffer, buffer_size);
  hpx_network_register_buffer(recv_buffer, buffer_size);

  clock_gettime(CLOCK_MONOTONIC, &begin_ts);

  if (rank == 0) {

    for (i = 0; i < iter_limit; i++) {
      snprintf(send_buffer, 50, "Message %d from proc 0", i);
      hpx_network_send_start(1, PING_TAG, send_buffer, buffer_size, 0, &send_request);
      hpx_network_wait(send_request);
      
      //      photon_wait_send_request_rdma(PONG_TAG);
      hpx_network_recv_start(-1, PONG_TAG, recv_buffer, buffer_size, 0, &recv_request);
      //hpx_network_recv_start(1, PONG_TAG, recv_buffer, buffer_size, 0, &recv_request);
      hpx_network_wait(recv_request);
      //printf("%02d: Receiver done waiting on recv %d\n", i, recv_request);
      //printf("Message from proc 1: <<%s>>\n", recv_buffer);


    }
  }
  else if (rank == 1) {
    //    strcpy(send_buffer, "Message from proc 1");


    for (i = 0; i < iter_limit; i++) {
      hpx_network_recv_start(0, PING_TAG, recv_buffer, buffer_size, 0, &recv_request);
      hpx_network_wait(recv_request);

      int str_length;
      snprintf(copy_buffer, 50, "At %d, received from proc 0 message: '", i);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], recv_buffer);
      str_length = strlen(copy_buffer);
      strcpy(&copy_buffer[str_length], "'");
      strcpy(send_buffer, copy_buffer);

      photon_post_send_request_rdma(0, buffer_size, PONG_TAG, &send_request);
      hpx_network_wait(send_request);
      //printf("%02d: Sender done waiting on photon_post_send_request_rdma %d\n", i, send_request);
      hpx_network_send_start(0, PONG_TAG, send_buffer, buffer_size, 0, &send_request);
      hpx_network_wait(send_request);
      //printf("%02d: Sender done waiting on send %d\n", i, send_request);
    }
  }

  clock_gettime(CLOCK_MONOTONIC, &end_ts);
  elapsed = ((end_ts.tv_sec * 1000000000) + end_ts.tv_nsec) - ((begin_ts.tv_sec * 1000000000) + begin_ts.tv_nsec);

  avg_oneway_latency = elapsed/((double)(iter_limit*2));

  printf("average oneway latency :   %f ms\n", avg_oneway_latency/1000000.0);

  hpx_network_unregister_buffer(send_buffer, buffer_size);
  hpx_network_unregister_buffer(recv_buffer, buffer_size);

  free(send_buffer);
  free(recv_buffer);
  free(copy_buffer);
#endif

  while (0) {
    /* TODO: wait for signal to exit... */
  }

  int * retval;
  retval = hpx_alloc(sizeof(int));
  *retval = 0;
  hpx_thread_exit((void*)retval);

  return NULL;
}


hpx_parchandler_t * hpx_parchandler_create() {
  hpx_config_t * cfg = NULL;
  hpx_parchandler_t * ph = NULL;

  cfg = hpx_alloc(sizeof(hpx_config_t));
  ph = hpx_alloc(sizeof(hpx_parchandler_t));

  if (cfg != NULL) {
    hpx_config_init(cfg);
    if (ph != NULL) {
      ph->ctx = hpx_ctx_create(cfg);
      /* TODO: error check */
      ph->thread = hpx_thread_create(ph->ctx, 0, _hpx_parchandler_main_pingpong, 0);
      /* TODO: error check */
    }
    else {
      __hpx_errno = HPX_ERROR_NOMEM;
    }
  }

  return ph;
}

void hpx_parchandler_destroy(hpx_parchandler_t * ph) {
  int *retval;

  hpx_thread_join(ph->thread, (void**)&retval);

  /* test code */
  /* TODO: remove */
  printf("Thread joined\n");
  printf("Value on return = %d\n", *retval);
  /* end test code */

  hpx_thread_destroy(ph->thread); // TODO: find out where this function has gone to...
  hpx_ctx_destroy(ph->ctx);
  hpx_free(ph);
}
