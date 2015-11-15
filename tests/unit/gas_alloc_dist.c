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

#include <hpx/hpx.h>
#include "tests.h"

static const int blocksize = 4 * sizeof(int);

typedef enum {
  _alloc = 0,
  _calloc
} _alloc_type;

typedef enum {
  _cyclic = 0,
  _blocked
} _dist_type;

static int _block_rank_handler(hpx_addr_t base) {
  hpx_addr_t target = hpx_thread_current_target();
  int id = hpx_addr_sub(target, base, blocksize) / blocksize;
  int block_rank = id * HPX_LOCALITY_ID;
  HPX_THREAD_CONTINUE(block_rank);
  return HPX_SUCCESS;
}
HPX_ACTION(HPX_DEFAULT, 0, _block_rank, _block_rank_handler, HPX_ADDR);

static void _init_handler(int *input, const size_t bytes) {
  *input = 0;
}
static HPX_ACTION(HPX_FUNCTION, 0, _init, _init_handler);

static void _add_handler(int *lhs, const int *rhs, size_t UNUSED) {
  *lhs += *rhs;
}
static HPX_ACTION(HPX_FUNCTION, 0, _add, _add_handler);

// Assuming n localities and 2 blocks per locality:
//
// _cyclic
// rank  0 0 1  1  2  2  3  3  . . n-1 n-1
// block 0 n 1 n+1 2 n+2 3 n+3 . . n-1 2n-1
//
// sum(rank_i * block_i) = (7n^3 - 9n^2 + 2n)/6
//
// _blocked
// rank  0 0 1 1 2 2 3 3 . . n-1  n-1
// block 0 1 2 3 4 5 6 7 . .  2n 2n-1
//
// sum(rank_i * block_i) = ???
// not sure if there's a closed-form expr for this
//
static int _verify(_dist_type dist) {
  int n = HPX_LOCALITIES;
  if (dist == _cyclic) {
    return ((7*n*n*n) - (9*n*n) + (2*n))/6;
  } else if (dist == _blocked) {
    int expected = 0;
    for (int i = 0; i < n; ++i) {
      expected += i * (i/2);
    }
    return expected;
  }
  return 0;
}

static int _run_test(_alloc_type alloc, _dist_type dist) {
  hpx_addr_t (*alloc_fn)(size_t, uint32_t, uint32_t);
  if (dist == _cyclic && alloc == _alloc) {
    alloc_fn = hpx_gas_alloc_cyclic;
  } else if (dist == _cyclic && alloc == _calloc) {
    alloc_fn = hpx_gas_calloc_cyclic;
  } else if (dist == _blocked && alloc == _alloc) {
    alloc_fn = hpx_gas_alloc_blocked;
  } else if (dist == _blocked && alloc == _calloc) {
    alloc_fn = hpx_gas_calloc_blocked;
  } else {
    printf("unknown dist or alloc type.\n");
    return HPX_ERROR;
  }

  int e = HPX_SUCCESS;
  int blocks = 2*HPX_LOCALITIES;
  hpx_addr_t sum_lco = hpx_lco_reduce_new(blocks, sizeof(int), _init, _add);
  hpx_addr_t data = alloc_fn(blocks, blocksize, 0);
  hpx_gas_bcast_with_continuation(_block_rank, data, blocks, 0, blocksize,
                                  hpx_lco_set_action, sum_lco, &data);
  int sum;
  hpx_lco_get(sum_lco, sizeof(sum), &sum);
  hpx_lco_delete(sum_lco, HPX_NULL);

  hpx_gas_free(data, HPX_NULL);

  int expected = _verify(dist);
  if (sum == expected) {
    e = HPX_SUCCESS;
  } else {
    printf("ERROR: expected %d got %d\n", expected, sum);
    e = HPX_ERROR;
  }
  CHECK( e );
  return e;
}

static int gas_alloc_cyclic_handler(void) {
  return _run_test(_alloc, _cyclic);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_alloc_cyclic,
                  gas_alloc_cyclic_handler);

static int gas_calloc_cyclic_handler(void) {
  return _run_test(_calloc, _cyclic);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_calloc_cyclic,
                  gas_calloc_cyclic_handler);

static int gas_alloc_blocked_handler(void) {
  return _run_test(_alloc, _blocked);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_alloc_blocked,
                  gas_alloc_blocked_handler);

static int gas_calloc_blocked_handler(void) {
  return _run_test(_calloc, _blocked);
}
static HPX_ACTION(HPX_DEFAULT, 0, gas_calloc_blocked,
                  gas_calloc_blocked_handler);

TEST_MAIN({
    ADD_TEST(gas_alloc_cyclic, 0);
    ADD_TEST(gas_calloc_cyclic, 0);
    // ADD_TEST(gas_alloc_blocked, 0);
    // ADD_TEST(gas_calloc_blocked, 0);
});
