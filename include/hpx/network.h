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

#include <stdlib.h>
#include "hpx/types.h"
#include "hpx/runtime.h"

#if HAVE_MPI
  #include <mpi.h>
#endif
#if HAVE_PHOTON
  #include <photon.h>
#endif

#define NETWORK_ANY_SOURCE -1
#define NETWORK_ANY_LENGTH -1

/**
 * Some basic underlying network types
 */

typedef struct network_request_t {
#if HAVE_MPI
  MPI_Request mpi;
#endif
#if HAVE_PHOTON
  uint32_t photon;
#endif
} network_request_t;

typedef struct network_status_t {
  int source;
  int count;
#if HAVE_MPI
  MPI_Status mpi;
#endif
#if HAVE_PHOTON
  struct photon_status_t photon;
#endif
} network_status_t;

/**
 * Network Operations
 */
typedef struct network_ops_t {
  /* Initialize the network layer */
  int (*init)(void);
  /* Shutdown and clean up the network layer */
  int (*finalize)(void);
  /* The network progress function */
  void (*progress)(void *data);
  int (*probe)(int src, int* flag, network_status_t* status);
  /* Send a raw payload */
  int (*send)(int dest, void *buffer, size_t len, network_request_t *request);
  /* Receive a raw payload */
  int (*recv)(int src, void *buffer, size_t len, network_request_t *request);
  /* RMA put */
  int (*put)(int dest, void *buffer, size_t len, network_request_t *request);
  /* RMA get */
  int (*get)(int dest, void *buffer, size_t len, network_request_t *request); 
  /* test for completion of communication */
  int (*test)(network_request_t *request, int *flag, network_status_t *status);
  /* pin memory for put/get */
  int (*pin)(void* buffer, size_t len);
  /* unpin memory for put/get */
  int (*unpin)(void* buffer, size_t len);
  /* get the physical address of the current locality */
  int (*phys_addr)(hpx_locality_t *l);
} network_ops_t;

extern network_ops_t default_net_ops;
extern network_ops_t mpi_ops;
extern network_ops_t photon_ops;

/**
 * Network Transport
 */
typedef struct network_trans_t {
  char           name[128];
  network_ops_t *ops;
  int           *flags;
  int            active;
} network_trans_t;

typedef struct network_mgr_t {
  /* Default transport. We do not want to walk a list when
   * multi-railing is turned off.  */
  network_trans_t *defaults;
  /* Number of transports in list, since we don't walk to traverse
     the list to know how many */
  int count;
  /* List of configured transports  */
  network_trans_t *trans;
} network_mgr_t;

/**
 * Default network operations
 */

/* Initialize the network layer */
int hpx_network_init(void);

/* Shutdown and clean up the network layer */
int hpx_network_finalize(void);

int hpx_network_probe(int source, int* flag, network_status_t* status);

/* Send a raw payload */
int hpx_network_send(int dest, void *buffer, size_t len, network_request_t* req);

/* Receive a raw payload */
int hpx_network_recv(int src, void *buffer, size_t len, network_request_t* req);

/* RMA put */
int hpx_network_put(int dest, void *buffer, size_t len, network_request_t* req);

/* RMA get */
int hpx_network_get(int src, void *buffer, size_t len, network_request_t* req);

/* Test for completion of put/get */
int hpx_network_test(network_request_t *request, int *flag, network_status_t *status);

/* pin memory for put/get */
int hpx_network_pin(void* buffer, size_t len);

/* unpin memory for put/get */
int hpx_network_unpin(void* buffer, size_t len);

/* get the physical address of the current locality */
int hpx_network_phys_addr(hpx_locality_t *l);

/* The network progress function */
void hpx_network_progress(void *data);

/**
 * Utility network operations
 */
void hpx_network_barrier();

#endif /* LIBHPX_NETWORK_H_ */

