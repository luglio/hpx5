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

// Goal of this testcase is to test the Parcels
// 1.  hpx_parcel_aquire()
// 2.  hpx_parcel_set_target()
// 3.  hpx_parcel_set_action()
// 4.  hpx_parcel_set_data()
// 5.  hpx_parcel_send_sync()
// 6.  hpx_parcel_release()
// 7.  hpx_parcel_send()
// 8.  hpx_parcel_get_action()
// 9.  hpx_parcel_get_target()
// 10. hpx_parcel_get_cont_action()
// 11. hpx_parcel_get_cont_target()
// 12. hpx_parcel_get_data()
// 13. hpx_parcel_set_cont_action()
// 14. hpx_parcel_set_cont_target()

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "hpx/hpx.h"
#include "tests.h"
#include "libsync/sync.h"

#define BUFFER_SIZE 128

typedef struct initBuffer {
  int  index;
  char message[BUFFER_SIZE];
} initBuffer_t;

static __thread unsigned seed = 0;

static hpx_addr_t rand_rank(void) {
  int r = rand_r(&seed);
  int n = hpx_get_num_ranks();
  return HPX_THERE(r % n);
}

static HPX_ACTION(_send, int *args) {
  int n = *args;
  //printf( "locality: %d, thread: %d, count: %d\n", hpx_get_my_rank(),
  //       hpx_get_my_thread_id(), n);
  //printf("count: %d\n", n);
  if (n-- <= 0) {
    //printf("terminating.\n");
    return HPX_SUCCESS;
  }

  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(int));
  hpx_parcel_set_target(p, rand_rank());
  hpx_parcel_set_action(p, _send);
  hpx_parcel_set_data(p, &n, sizeof(n));

  assert(hpx_gas_try_pin(hpx_parcel_get_target(p), NULL));

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Test code -- Parcels
static HPX_ACTION(parcel_create, void *UNUSED) {
  int n = 0;
  printf("Starting the parcel create test\n");
  // Start the timer
  hpx_time_t t1 = hpx_time_now();

  hpx_addr_t completed = hpx_lco_and_new(1);
  hpx_call(HPX_HERE, _send, completed, &n, sizeof(n));
  hpx_lco_wait(completed);
  hpx_lco_delete(completed, HPX_NULL);

  printf(" Elapsed: %g\n", hpx_time_elapsed_ms(t1));
  return HPX_SUCCESS;
}

// Test code for parcel creation with arguments and parcel set and get action
hpx_addr_t _partner(void) {
  int rank = hpx_get_my_rank();
  int ranks = hpx_get_num_ranks();
  return HPX_THERE((rank) ? 0 : ranks - 1);
}

static HPX_ACTION(_send_data, const initBuffer_t *args) {
  //printf("Received message = '%s', %d from (%d, %d)\n", args->message,
  //       args->index, hpx_get_my_rank(), hpx_get_my_thread_id());
  return HPX_SUCCESS;
}

static HPX_ACTION(parcel_get_action, void *UNUSED) {
  printf("Testing the parcel create with arguments\n");
  initBuffer_t args = {
    .index = hpx_get_my_rank(),
    .message = "parcel creation test"
  };

  hpx_addr_t to = _partner();

  hpx_parcel_t *p = hpx_parcel_acquire(&args, sizeof(args));
  hpx_parcel_set_action(p, _send_data);

  hpx_action_t get_act = hpx_parcel_get_action(p);
  assert_msg(get_act == _send_data, "Error creating parcel - wrong action");

  hpx_parcel_set_data(p, &args, sizeof(args));
  hpx_parcel_set_target(p, to);

  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Test code to test parcel get data functions - The hpx_parcel_get_data gets
// the data buffer for a parcel. The data for a parcel can be written to
// directly, which in some cases may allow one to avoid an extra copy.
static HPX_ACTION(parcel_get_data, void *UNUSED) {
  printf("Testing the parcel get data function\n");
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _send_data);
  initBuffer_t *args = (initBuffer_t *)hpx_parcel_get_data(p);
  args->index = hpx_get_my_rank();
  strcpy(args->message,"parcel get data test");
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}

// Testcase to test hpx_parcel_release function which explicitly releases a
// a parcel. The input argument must correspond to a parcel pointer returned
// from hpx_parcel_acquire
static HPX_ACTION(parcel_release, void *UNUSED) {
  printf("Testing the parcel release function\n");
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(initBuffer_t));
  hpx_parcel_set_target(p, HPX_HERE);
  hpx_parcel_set_action(p, _send_data);
  initBuffer_t *args = (initBuffer_t *)hpx_parcel_get_data(p);
  args->index = hpx_get_my_rank();
  strcpy(args->message,"parcel release test");
  hpx_parcel_release(p);
  return HPX_SUCCESS;
}

TEST_MAIN({
  ADD_TEST(parcel_create);
  ADD_TEST(parcel_get_action);
  ADD_TEST(parcel_get_data);
  ADD_TEST(parcel_release);
});