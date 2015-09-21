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

#include "hpx/hpx.h"
#include "tests.h"

static int _main_handler(void) {
  hpx_exit(HPX_SUCCESS);
  return HPX_SUCCESS;
}
static HPX_ACTION(HPX_DEFAULT, 0, _main, _main_handler);

int main(int argc, char *argv[]) {
  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "failed to initialize HPX.\n");
    return 1;
  }
  int e = hpx_run(&_main);
  printf("1 hpx_run returned %d.\n", e);

  // remove the following block to call hpx_run() twice:
  hpx_finalize();
  return 77;

  e = hpx_run(&_main);
  printf("2 hpx_run returned %d.\n", e);
  hpx_finalize();
  return e;
}
