// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2015, Trustees of Indiana University,
//  All rights reserved.
//
//  This software may be modified and distributed under the terms of the BSD
//  license.  See the COPYING file for details.
//
//  This software was created at the Indiana University Center for Research in
//  Extreme Scale Technologies (CREST).
// =============================================================================
#ifndef LIBHPX_NETWORK_ISIR_XPORT_H
#define LIBHPX_NETWORK_ISIR_XPORT_H

#include <libhpx/config.h>

struct boot;
struct gas;

typedef struct isir_xport {
  hpx_transport_t type;
  void   (*delete)(void *xport);

  void   (*check_tag)(int tag);
  size_t (*sizeof_request)(void);
  size_t (*sizeof_status)(void);
  void   (*clear)(void *request);
  int    (*cancel)(void *request, int *cancelled);
  int    (*wait)(void *request, void *status);
  int    (*isend)(int to, const void *from, unsigned n, int tag, void *request);
  int    (*irecv)(void *to, size_t n, int tag, void *request);
  int    (*iprobe)(int *tag);
  void   (*finish)(void *request, int *src, int *bytes);
  void   (*testsome)(int n, void *requests, int *cnt, int *out, void *statuses);
  int    (*pin)(void *xport, void *base, size_t bytes, void *key);
  int    (*unpin)(void *xport, void *base, size_t bytes);
} isir_xport_t;

isir_xport_t *isir_xport_new_mpi(const config_t *cfg, struct gas *gas)
  HPX_INTERNAL;

isir_xport_t *isir_xport_new(const config_t *cfg, struct gas *gas)
  HPX_INTERNAL;

static inline void isir_xport_delete(isir_xport_t *xport) {
  xport->delete(xport);
}

#endif // LIBHPX_NETWORK_ISIR_XPORT_H