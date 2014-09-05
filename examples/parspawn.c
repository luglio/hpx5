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

#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>
#include <unistd.h>
#include "hpx/hpx.h"


/// ----------------------------------------------------------------------------
/// @file examples/hpx/parspawn.c
/// This file implements a parallel (tree) spawn, that uses HPX
/// threads to spawn many NOP operations in parallel.
/// ----------------------------------------------------------------------------

static void _usage(FILE *stream) {
  fprintf(stream, "Usage: parspawn [options] NUMBER\n"
          "\t-c, number of cores to run on\n"
          "\t-t, number of scheduler threads\n"
          "\t-D, all localities wait for debugger\n"
          "\t-d, wait for debugger at specific locality\n"
          "\t-h, this help display\n");
}

static hpx_action_t _nop     = 0;
static hpx_action_t _main    = 0;


// The empty action
static int _nop_action(void *args) {
  hpx_thread_exit(HPX_SUCCESS);
}

static int _main_action(int *args) {
  int n = *args;
  printf("parspawn(%d)\n", n); fflush(stdout);

  hpx_time_t now = hpx_time_now();
  hpx_par_for_sync(_nop, 0, n, 8, 1000, 0, NULL, 0, 0);
  double elapsed = hpx_time_elapsed_ms(now)/1e3;

  printf("seconds: %.7f\n", elapsed);
  printf("localities:   %d\n", HPX_LOCALITIES);
  printf("threads:      %d\n", HPX_THREADS);
  hpx_shutdown(HPX_SUCCESS);
}

/// ----------------------------------------------------------------------------
/// The main function parses the command line, sets up the HPX runtime system,
/// and initiates the first HPX thread to perform parspawn(n).
///
/// @param argc    - number of strings
/// @param argv[0] - parspawn
/// @param argv[1] - number of cores to use, '0' means use all
/// @param argv[2] - n
/// ----------------------------------------------------------------------------
int
main(int argc, char *argv[])
{
  hpx_config_t cfg = {
    .cores       = 0,
    .threads     = 0,
    .stack_bytes = 0,
    .gas         = HPX_GAS_NOGLOBAL
  };

  int opt = 0;
  while ((opt = getopt(argc, argv, "c:t:d:Dh")) != -1) {
    switch (opt) {
     case 'c':
      cfg.cores = atoi(optarg);
      break;
     case 't':
      cfg.threads = atoi(optarg);
      break;
     case 'D':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = HPX_LOCALITY_ALL;
      break;
     case 'd':
      cfg.wait = HPX_WAIT;
      cfg.wait_at = atoi(optarg);
      break;
     case 'h':
      _usage(stdout);
      return 0;
     case '?':
     default:
      _usage(stderr);
      return -1;
    }
  }

  argc -= optind;
  argv += optind;

  int n = 0;
  switch (argc) {
   case 0:
    fprintf(stderr, "\nMissing fib number.\n"); // fall through
   default:
    _usage(stderr);
    return -1;
   case 1:
     n = atoi(argv[0]);
     break;
  }

  if (hpx_init(&cfg)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  // register the actions
  _nop     = HPX_REGISTER_ACTION(_nop_action);
  _main    = HPX_REGISTER_ACTION(_main_action);

  // run the main action
  return hpx_run(_main, &n, sizeof(n));
}
