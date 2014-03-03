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
#ifndef LIBHPX_TRANSPORT_H
#define LIBHPX_TRANSPORT_H

#include "attributes.h"

typedef struct transport_status transport_status_t;
typedef struct transport_request transport_request_t;
typedef struct transport transport_t;

/// ----------------------------------------------------------------------------
/// The transport interface is defined as an abstract class.
/// ----------------------------------------------------------------------------
struct transport {
  int (*init)(void);
  void (*fini)(void);
  void (*progress)(void *data);
  int (*probe)(int src, int *flag, transport_status_t *status);
  int (*send)(int dest, void *buffer, unsigned size, transport_request_t *request);
  int (*recv)(int src, void *buffer, unsigned size, transport_request_t *request);
  int (*test_sendrecv)(transport_request_t *request, int *flag, transport_status_t *status);
  int (*test_send)(transport_request_t *request, int *flag, transport_status_t *status);
  int (*test_recv)(transport_request_t *request, int *flag, transport_status_t *status);
  int (*put)(int dest, void *buffer, unsigned len, transport_request_t *request);
  int (*get)(int dest, void *buffer, unsigned len, transport_request_t *request);
  int (*test_putget)(transport_request_t *request, int *flag, transport_status_t *status);
  int (*test_put)(transport_request_t *request, int *flag, transport_status_t *status);
  int (*test_get)(transport_request_t *request, int *flag, transport_status_t *status);
  int (*pin)(void* buffer, unsigned len);
  int (*unpin)(void* buffer, unsigned len);
  int (*phys_addr)(int *locality);
  unsigned (*get_transport_bytes)(unsigned n);
  void (*barrier)(void);
  void (*delete)(transport_t *);
};


/// concrete network constructors
HPX_INTERNAL transport_t *transport_new_photon(void);
HPX_INTERNAL transport_t *transport_new_mpi(void);
HPX_INTERNAL transport_t *transport_new_smp(void);

/// selects the best transport available.
HPX_INTERNAL transport_t *transport_new(void);

#endif // LIBHPX_TRANSPORT_H
