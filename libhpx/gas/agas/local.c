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
#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include "agas.h"
#include "btt.h"
#include "gva.h"

hpx_addr_t
agas_local_alloc(void *gas, uint32_t bytes, uint32_t boundary) {
  // use the local allocator to get some memory that is part of the global
  // address space
  uint32_t align = ceil_log2_32(bytes);
  dbg_assert(align < 32);
  uint32_t padded = UINT32_C(1) << align;
  uint32_t aligned = max_u32(boundary, padded);
  void *lva = global_memalign(aligned, padded);

  agas_t *agas = gas;
  gva_t gva = agas_lva_to_gva(gas, lva, padded);
  btt_insert(agas->btt, gva, here->rank, lva, 1);
  return gva.addr;
}

hpx_addr_t
agas_local_calloc(void *gas, size_t nmemb, size_t size, uint32_t boundary) {
  uint32_t align = ceil_log2_32(size);
  dbg_assert(align < 32);
  uint32_t padded = UINT32_C(1) << align;
  uint32_t aligned = max_u32(boundary, padded);

  char *lva = global_memalign(aligned, nmemb * padded);
  memset(lva, 0, nmemb * padded);

  agas_t *agas = gas;
  gva_t gva = agas_lva_to_gva(gas, lva, padded);
  hpx_addr_t base = gva.addr;
  for (int i = 0; i < nmemb; i++) {
    btt_insert(agas->btt, gva, here->rank, lva, nmemb);
    lva += padded;
    gva.bits.offset += padded;
  }
  return base;
}

int64_t
agas_local_sub(const agas_t *agas, gva_t lhs, gva_t rhs, uint32_t bsize) {
  uint64_t bits = lhs.bits.size;
  uint64_t mask = (UINT64_C(1) << bits) - 1;
  uint64_t plhs = lhs.bits.offset & mask;
  uint64_t prhs = rhs.bits.offset & mask;
  uint64_t blhs = lhs.bits.offset >> bits;
  uint64_t brhs = rhs.bits.offset >> bits;
  return (plhs - prhs) + (blhs - brhs) * bsize;
}

hpx_addr_t
agas_local_add(const agas_t *agas, gva_t gva, int64_t n, uint32_t bsize) {
  int64_t blocks = n / bsize;
  int64_t bytes = n % bsize;
  uint64_t block_size = (UINT64_C(1) << gva.bits.size);
  uint64_t addr = gva.addr + blocks * block_size + bytes;
  dbg_assert((addr & (block_size - 1)) < bsize);
  return addr;
}
