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

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hpx/hpx.h>

#define MAX_BYTES        1024*1024*1
#define SKIP_LARGE       10
#define LOOP_LARGE       100

int skip = 1000;
int loop = 10000;

#define BENCHMARK "HPX COST OF GAS ALLOCATIONS (ms)"

#define HEADER "# " BENCHMARK "\n"
#define FIELD_WIDTH 10
#define HEADER_FIELD_WIDTH 5
/// This file tests cost of GAS operations
static void usage(FILE *stream) {
  fprintf(stream, "Usage: time_gas_alloc [options]\n"
          "\t-h, this help display\n");
  hpx_print_help();
  fflush(stream);
}

static hpx_action_t _main    = 0;

static int _main_action(void *args, size_t n) {
  hpx_addr_t local, global, calloc_global;
  hpx_time_t t;
  int size = HPX_LOCALITIES;
  int ranks = hpx_get_num_ranks();
  uint32_t blocks = size;

  fprintf(stdout, HEADER);
  fprintf(stdout, "localities: %d, ranks and blocks per rank = %d, %d\n",
                  size, ranks, blocks/ranks);

  fprintf(stdout, "%s\t%*s%*s%*s%*s%*s%*s\n", "# Size ", HEADER_FIELD_WIDTH,
           " LOCAL_ALLOC ", HEADER_FIELD_WIDTH, " FREE ", HEADER_FIELD_WIDTH,
           " GLOBAL_ALLOC ", HEADER_FIELD_WIDTH, " FREE ", HEADER_FIELD_WIDTH,
           " GLOBAL_CALLOC ", HEADER_FIELD_WIDTH, " FREE ");

  for (size_t size = 1; size <= MAX_BYTES; size*=2) {
    t = hpx_time_now();
    local = hpx_gas_alloc_local(1, size, 0);
    fprintf(stdout, "%-*zu%*g", 10,  size, FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    hpx_gas_free(local, HPX_NULL);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    global = hpx_gas_alloc_cyclic(blocks, size, 0);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    hpx_gas_free(global, HPX_NULL);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    calloc_global = hpx_gas_calloc_cyclic(blocks, size, 0);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));

    t = hpx_time_now();
    hpx_gas_free(calloc_global, HPX_NULL);
    fprintf(stdout, "%*g", FIELD_WIDTH, hpx_time_elapsed_ms(t));
    fprintf(stdout, "\n");
  }
  hpx_exit(HPX_SUCCESS);
}

int main(int argc, char *argv[]) {
  // Register the main action
  HPX_REGISTER_ACTION(HPX_DEFAULT, HPX_MARSHALLED, _main, _main_action, HPX_POINTER, HPX_SIZE_T);

  if (hpx_init(&argc, &argv)) {
    fprintf(stderr, "HPX: failed to initialize.\n");
    return 1;
  }

  int opt = 0;
  while ((opt = getopt(argc, argv, "h?")) != -1) {
    switch (opt) {
     case 'h':
      usage(stdout);
      return 0;
     case '?':
     default:
      usage(stderr);
      return -1;
    }
  }

  fprintf(stdout, "Starting the cost of GAS Allocation benchmark\n");


  // run the main action
  int e = hpx_run(&_main, NULL, 0);
  hpx_finalize();
  return e;
}
