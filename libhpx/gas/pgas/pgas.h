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
#ifndef LIBHPX_GAS_PGAS_H
#define LIBHPX_GAS_PGAS_H

#include <hpx/hpx.h>
#include <hpx/attributes.h>

extern struct heap *global_heap;

/// Called by each OS thread (pthread) to initialize and setup the thread local
/// structures required for it to use PGAS.
///
/// @note This currently only works with the global_heap object. A more flexible
///       solution might be appropriate some day.
///
/// @note Implemented in malloc.c.
///
/// @returns LIBHPX_OK, or LIBHPX_ERROR if there is an error.
///
int pgas_join(void)
  HPX_INTERNAL;

/// Called by each OS thread (pthread) to clean up any thread local structures
/// that were initialized during pgas_join().
///
/// @note Implemented in malloc.c.
///
void pgas_leave(void)
  HPX_INTERNAL;

/// Implementation of the distributed functionality that supports cyclic malloc
/// and calloc.
/// @{

/// The structure that describes the cyclic operations, we just need to know the
/// overall number of bytes to allocate, and the block size.
typedef struct {
  size_t n;
  uint32_t bsize;
} pgas_alloc_args_t;


/// Asynchronous entry point for alloc.
extern hpx_action_t pgas_cyclic_alloc;


/// Asynchronous entry point for calloc.
extern hpx_action_t pgas_cyclic_calloc;


/// Asynchronous entry point for free.
extern hpx_action_t pgas_free;


/// Synchronous entry point for alloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns A global address representing the base of the allocation, or
///          HPX_NULL if there is an error.
hpx_addr_t pgas_cyclic_alloc_sync(size_t n, uint32_t bsize)
  HPX_INTERNAL;


/// Synchronous entry point for calloc.
///
/// @param            n The total number of bytes to allocate.
/// @param        bsize The size of each block, in bytes.
///
/// @returns A global address representing the base of the allocation, or
///          HPX_NULL if there is an error.
hpx_addr_t pgas_cyclic_calloc_sync(size_t n, uint32_t bsize)
  HPX_INTERNAL;


/// @}


/// Convert a global address into a local virtual address.
///
/// @param          gva The global address.
///
/// @returns The corresponding local virtual address.
void *pgas_gva_to_lva(hpx_addr_t gva)
  HPX_INTERNAL;


/// Convert a local address into a global virtual address.
///
/// @param          gva The global address.
///
/// @returns The corresponding local virtual address.
hpx_addr_t pgas_lva_to_gva(void *lva)
  HPX_INTERNAL;


#endif // LIBHPX_GAS_PGAS_H
