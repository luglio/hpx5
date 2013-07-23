/*
 ====================================================================
  High Performance ParalleX Library (libhpx)
  
  Network Functions
  network.h

  Copyright (c) 2013, Trustees of Indiana University 
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/

#pragma once
#ifndef LIBHPX_NETWORK_H_
#define LIBHPX_NETWORK_H_

#if HAVE_MPI
  #include <mpi.h>
#endif
#if HAVE_PHOTON
  #include <photon.h>
#endif

typedef struct network_mgr_t {
  /* Default transport. We do not want to walk a list when
   * multi-railing is turned off.  */
  network_trans_t *default;
  /* List of configured transports  */
  network_trans_t *trans;
} network_mgr_t;


/**
 * Network Transport
 */
typedef struct network_trans_t {
  char           name[128];
  network_ops_t *ops;
  int           *flags;
  int            active;
} network_trans_t;

/**
 * Network Operations
 */
typedef struct network_ops_t {
  /* Initialize the network layer */
  int (*init)(void);
  /* Shutdown and clean up the network layer */
  void (*finalize)(void);
  /* The network progress function */
  void (*progress)(void *data);
  /* Send a raw payload */
  int (*send)(int peer, void *payload, size_t len);
  /* Receive a raw payload */
  int (*recv)(int peer, void *payload, size_t len);
  /* test for completion of send or receive */
  int (*sendrecv_test)(comm_request_t *request, int *flag, comm_status_t *status);  
  /* RMA put */
  int (*put)(int peer, void *dst, void *src, size_t len);
  /* RMA get */
  int (*get)(void *dst, int peer, void *src, size_t len);
  /* test for completion of put or get */
  int (*putget_test)(comm_request_t *request, int *flag, comm_status_t *status);
  /* pin memory for put/get */
  int (*pin)(void* buffer, size_t len);
  /* unpin memory for put/get */
  int (*unpin)(void* buffer, size_t len);
} network_ops_t;

typedef struct comm_request_t {
  /* TODO: deal with case where there is no mpi */
  MPI_Request mpi;
  uint32_t photon;
} comm_request_t;

typedef struct comm_status_t {
  /* TODO: deal with case where there is no mpi */
  MPI_Status mpi;
  MPI_Status photon; /* not a mistake - Photon uses MPI status */
}

/**
 * Default network operations
 */

/* Initialize the network layer */
int hpx_network_init(void);

/* Shutdown and clean up the network layer */
void hpx_network_finalize(void);

/* Send a raw payload */
int hpx_network_send(int peer, void *src, size_t len);

/* Receive a raw payload */
int hpx_network_recv(int peer, void *dst, size_t len);

/* Test for completion of send/recv */
int hpx_sendrecv_test(comm_request_t *request, int *flag, comm_status_t *status);

/* RMA put */
int hpx_network_put(int peer, void *dst, void *src, size_t len);

/* RMA get */
int hpx_network_get(void *dst, int peer, void *src, size_t len);

/* Test for completion of put/get */
int hpx_putget_test(comm_request_t *request, int *flag, comm_status_t *status);

/* pin memory for put/get */
int hpx_network_pin(void* buffer, size_t len);

/* unpin memory for put/get */
int hpx_network_unpin(void* buffer, size_t len);

/* The network progress function */
void hpx_network_progress(void *data);


#endif /* LIBHPX_NETWORK_H_ */
