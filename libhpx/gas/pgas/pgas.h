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

//  ----------------------------------------------------------------------------
/// Declare the Address Space interface functions for PGAS. These are
/// implemented in malloc.c.
//  ----------------------------------------------------------------------------
/// @{
int   pgas_join(void) HPX_INTERNAL;
void  pgas_leave(void) HPX_INTERNAL;

void *pgas_global_malloc(size_t bytes) HPX_INTERNAL;
void  pgas_global_free(void *ptr) HPX_INTERNAL;
void *pgas_global_calloc(size_t nmemb, size_t size) HPX_INTERNAL;
void *pgas_global_realloc(void *ptr, size_t size) HPX_INTERNAL;
void *pgas_global_valloc(size_t size) HPX_INTERNAL;
void *pgas_global_memalign(size_t boundary, size_t size) HPX_INTERNAL;
int   pgas_global_posix_memalign(void **memptr, size_t alignment, size_t size) HPX_INTERNAL;

void *pgas_local_malloc(size_t bytes) HPX_INTERNAL;
void  pgas_local_free(void *ptr) HPX_INTERNAL;
void *pgas_local_calloc(size_t nmemb, size_t size) HPX_INTERNAL;
void *pgas_local_realloc(void *ptr, size_t size) HPX_INTERNAL;
void *pgas_local_valloc(size_t size) HPX_INTERNAL;
void *pgas_local_memalign(size_t boundary, size_t size) HPX_INTERNAL;
int   pgas_local_posix_memalign(void **memptr, size_t alignment, size_t size) HPX_INTERNAL;
/// @}

typedef struct {
  size_t n;
  uint32_t bsize;
} alloc_handler_args_t;

extern hpx_action_t act_pgas_cyclic_alloc_handler;
extern hpx_action_t act_pgas_cyclic_calloc_handler;

void pgas_register_actions(void) HPX_INTERNAL;


int pgas_cyclic_alloc_handler(alloc_handler_args_t *args)
  HPX_INTERNAL;



int pgas_cyclic_calloc_handler(alloc_handler_args_t *args)
  HPX_INTERNAL;




#endif // LIBHPX_GAS_PGAS_H
