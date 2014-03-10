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
#include "config.h"
#endif

/// ----------------------------------------------------------------------------
/// @file libhpx/locality.c
/// ----------------------------------------------------------------------------
#include <stdlib.h>
#include <unistd.h>                             // sysconf(...)
#include "hpx.h"
#include "locality.h"
#include "manager.h"

hpx_addr_t HPX_HERE = { NULL, -1 };

/// Locality data.
static manager_t *_manager = NULL;

///
int
locality_startup(const hpx_config_t *cfg) {
  _manager = manager_new();
  if (!_manager)
    return 1;
  HPX_HERE.rank = _manager->rank;
  return HPX_SUCCESS;
}


void
locality_shutdown(void) {
  _manager->delete(_manager);
}


int
locality_get_rank(void) {
  return (_manager) ? _manager->rank : -1;
}


int
locality_get_n_ranks(void) {
  return (_manager) ? _manager->n_ranks : -1;
}


/// get the max number of processing units
int
locality_get_n_processors(void) {
  return sysconf(_SC_NPROCESSORS_ONLN);
}


hpx_action_t
locality_action_register(const char *id, hpx_action_handler_t f) {
  return (hpx_action_t)f;
}


hpx_action_handler_t
locality_action_lookup(hpx_action_t key) {
  return (hpx_action_handler_t)key;
}

int
hpx_get_my_rank(void) {
  return locality_get_rank();
}


int
hpx_get_num_ranks(void) {
  return locality_get_n_ranks();
}
