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

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "tests.h"

/// Initialize a double zero.
static void _initDouble(double *input, const size_t bytes) {
  *input = 0.0;
}

static void _addDouble(double *lhs, const double *rhs, size_t UNUSED) {
  *lhs += *rhs;
}

static int _reduce_handler(double data) {
  HPX_THREAD_CONTINUE(data);
}
static HPX_ACTION_DEF(DEFAULT, _reduce_handler, _reduce, HPX_DOUBLE);

static HPX_ACTION(lco_reduce, void *UNUSED) {
  static const double data = 3141592653.58979;
  static const int nDoms = 91;
  static const int cycles = 10;
  hpx_addr_t domain = hpx_gas_calloc_cyclic(nDoms, sizeof(int), 0);
  hpx_addr_t newdt = hpx_lco_reduce_new(nDoms, sizeof(double),
                                        (hpx_monoid_id_t)_initDouble,
                                        (hpx_monoid_op_t)_addDouble);

  for (int i = 0, e = cycles; i < e; ++i) {
    printf("reducing iteration %d \n", i);
    hpx_addr_t and = hpx_lco_and_new(nDoms);
    for (int j = 0, e = nDoms; j < e; ++j) {
      hpx_addr_t block = hpx_addr_add(domain, sizeof(int) * j, sizeof(int));
      hpx_call_async(block, _reduce, and, newdt, &data);
    }
    hpx_lco_wait(and);
    hpx_lco_delete(and, HPX_NULL);

    // Get the gathered value, and print the debugging string.
    double ans;
    hpx_lco_get(newdt, sizeof(ans), &ans);
    if (fabs(nDoms * data - ans) > 1) {
      fprintf(stderr, "expected %f, got %f\n", nDoms * data, ans);
      exit(EXIT_FAILURE);
    }
    hpx_lco_reset_sync(newdt);
  }

  hpx_lco_delete(newdt, HPX_NULL);
  hpx_gas_free(domain, HPX_NULL);

  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(lco_reduce);
});
