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
#ifndef LIBHPX_GAS_AGAS_CHUNK_TABLE_H
#define LIBHPX_GAS_AGAS_CHUNK_TABLE_H

#ifdef __cplusplus
extern "C" {
#endif

/// The chunk table provides inverse mapping from an lva chunk to its base
/// offset in the virtual address space.

#include <hpx/attributes.h>

HPX_INTERNAL void *chunk_table_new(size_t size);
HPX_INTERNAL void chunk_table_delete(void *table);

HPX_INTERNAL uint64_t chunk_table_lookup(void *table, void *chunk);
HPX_INTERNAL void chunk_table_insert(void *table, void *chunk, uint64_t base);
HPX_INTERNAL void chunk_table_remove(void *table, void *chunk);

#ifdef __cplusplus
}
#endif

#endif // LIBHPX_GAS_AGAS_CHUNK_TABLE_H