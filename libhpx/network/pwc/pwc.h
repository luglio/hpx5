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
#ifndef LIBHPX_NETWORK_PWC_PWC_H
#define LIBHPX_NETWORK_PWC_PWC_H

#include <hpx/attributes.h>
#include <libhpx/network.h>

/// Forward declarations.
/// @{
struct boot;
struct config;
struct gas;
struct parcel_emulator;
struct pwc_xport;
struct send_buffer;
/// @}

typedef struct {
  network_t                   vtable;
  const struct config           *cfg;
  struct pwc_xport            *xport;
  struct parcel_emulator    *parcels;
  struct send_buffer   *send_buffers;
  struct heap_segment *heap_segments;
} pwc_network_t;

network_t *network_pwc_funneled_new(const struct config *cfg, struct boot *boot,
                                    struct gas *gas)
  HPX_MALLOC HPX_INTERNAL;


#endif
