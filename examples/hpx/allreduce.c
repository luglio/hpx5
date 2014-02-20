/*
 ====================================================================
  High Performance ParalleX Library (libhpx)

  Pingong example
  examples/hpx/pingpong.c

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
 ====================================================================
*/
#include <assert.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <hpx.h>

#define T int

static T value;

static hpx_action_t set_value = HPX_ACTION_NULL;
static hpx_action_t get_value = HPX_ACTION_NULL;
static hpx_action_t allreduce = HPX_ACTION_NULL;

static T
sum(T count, T values[count]) {
  T total = 0;
  for (int i = 0; i < count; ++i, total += values[i])
    ;
  return total;
}

static int
action_get_value(void *args) {
  hpx_thread_exit(HPX_SUCCESS, &value, sizeof(value));
}

static int
action_set_value(void *args) {
  value = *(T*)args;
  printf("At rank %d received value %lld\n", hpx_get_my_rank(), (long long)value);
  hpx_thread_exit(HPX_SUCCESS, NULL, 0);
}

static int
action_allreduce(void *unused) {
  int num_ranks = hpx_get_num_ranks();
  int my_rank = hpx_get_my_rank();
  assert(my_rank == 0);

  T values[num_ranks];
  hpx_addr_t futures[num_ranks];

  for (int i = 0; i < num_ranks; ++i) {
    futures[i] = hpx_future_new(sizeof(T));
    hpx_parcel_t *p = hpx_parcel_acquire(0);
    hpx_parcel_set_action(p, get_value);
    hpx_parcel_set_target(p, hpx_addr_from_rank(i));
    hpx_parcel_set_cont(p, futures[i]);
    hpx_parcel_send(p);
  }

  hpx_thread_wait_all(num_ranks, futures, (void**)values);

  value = sum(num_ranks, values);

  for (int i = 0; i < num_ranks; ++i) {
    hpx_future_delete(futures[i]);
    futures[i] = hpx_future_new(0);
    hpx_parcel_t *p = hpx_parcel_acquire(sizeof(value));
    hpx_parcel_set_action(p, set_value);
    hpx_parcel_set_target(p, hpx_addr_from_rank(i));
    hpx_parcel_set_cont(p, futures[i]);
    *(T*)hpx_parcel_get_data(p) = value;
    hpx_parcel_send(p);
  }

  hpx_thread_wait_all(num_ranks, futures, NULL);
  hpx_shutdown(0);
}

int
main(int argc, char** argv) {
  int success = hpx_init(argc, argv);
  if (success != 0) {
    printf("Error %d in hpx_init!\n", success);
    exit(-1);
  }

  // register action for parcel
  set_value = hpx_action_register("set_value", action_set_value);
  get_value = hpx_action_register("get_value", action_get_value);
  allreduce = hpx_action_register("allreduce", action_allreduce);

  // Initialize the values that we want to reduce
  value = hpx_get_my_rank();

  return hpx_run(allreduce, NULL, 0);
}
