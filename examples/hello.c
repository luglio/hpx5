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

#include <stdio.h>
#include <hpx/hpx.h>

static int _hello_action(void *args) {
  printf("Hello World from %u.\n", hpx_get_my_rank());
  hpx_shutdown(0);
}

int main(int argc, char * const argv[argc]) {
  if (hpx_init(NULL) != 0)
    return -1;
  hpx_action_t hello = HPX_REGISTER_ACTION(_hello_action);
  return hpx_run(hello, NULL, 0);
}
