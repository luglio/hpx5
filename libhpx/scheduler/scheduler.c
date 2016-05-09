// =============================================================================
//  High Performance ParalleX Library (libhpx)
//
//  Copyright (c) 2013-2016, Trustees of Indiana University,
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

#include <stdlib.h>
#include <hpx/builtins.h>
#include <libhpx/action.h>
#include <libhpx/config.h>
#include <libhpx/debug.h>
#include <libhpx/gas.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/memory.h>
#include <libhpx/network.h>
#include <libhpx/rebalancer.h>
#include <libhpx/scheduler.h>
#include "thread.h"

static void _shutdown(scheduler_t *this) {
  pthread_mutex_lock(&this->lock);
  sync_store(&this->stopped, SCHED_SHUTDOWN, SYNC_RELEASE);
  this->state = SCHED_SHUTDOWN;
  pthread_cond_broadcast(&this->running);
  pthread_mutex_unlock(&this->lock);
}

scheduler_t *scheduler_new(const config_t *cfg) {
  thread_set_stack_size(cfg->stacksize);

  const int workers = cfg->threads;
  scheduler_t *this = NULL;
  size_t bytes = sizeof(*this) + workers * sizeof(worker_t);
  if (posix_memalign((void**)&this, HPX_CACHELINE_SIZE, bytes)) {
    dbg_error("could not allocate a scheduler.\n");
    return NULL;
  }

  sync_store(&this->stopped, SCHED_STOP, SYNC_RELEASE);
  sync_store(&this->next_tls_id, 0, SYNC_RELEASE);
  this->n_workers = workers;
  this->n_active = workers;

  this->state = SCHED_STOP;
  this->lock = (pthread_mutex_t)PTHREAD_MUTEX_INITIALIZER;
  this->running = (pthread_cond_t)PTHREAD_COND_INITIALIZER;

  // This thread can allocate even though it's not a scheduler thread.
  as_join(AS_REGISTERED);
  as_join(AS_GLOBAL);
  as_join(AS_CYCLIC);

  // Initialize the worker data structures.
  for (int i = 0, e = workers; i < e; ++i) {
    worker_init(&this->workers[i], this, i);
  }

  // Start the worker threads.
  for (int i = 0, e = workers; i < e; ++i) {
    if (worker_create(&this->workers[i])) {
      log_error("failed to initialize a worker during scheduler_new.\n");
      scheduler_delete(this);
      return NULL;
    }
  }

  log_sched("initialized a new scheduler.\n");
  return this;
}

void scheduler_delete(scheduler_t *this) {
  _shutdown(this);

  // join all of the worker threads
  for (int i = 0, e = this->n_workers; i < e; ++i) {
    worker_join(&this->workers[i]);
  }

  // clean up all of the worker data structures
  for (int i = 0, e = this->n_workers; i < e; ++i) {
    worker_fini(&this->workers[i]);
  }

  free(this);
  as_leave();
}

worker_t *scheduler_get_worker(scheduler_t *this, int id) {
  assert(id >= 0);
  assert(id < this->n_workers);
  worker_t *w = &this->workers[id];
  assert(((uintptr_t)w & (HPX_CACHELINE_SIZE - 1)) == 0);
  return w;
}

int scheduler_start(scheduler_t *this, hpx_parcel_t *p, int spmd)
{
  if (spmd || here->rank == 0) {
    scheduler_spawn_at(p, 0);
  }

  // switch the state of the running condition
  pthread_mutex_lock(&this->lock);
  sync_store(&this->stopped, SCHED_RUN, SYNC_RELEASE);
  int state = (this->state = SCHED_RUN);
  pthread_cond_broadcast(&this->running);
  while ((state = this->state) == SCHED_RUN) {
    pthread_cond_wait(&this->running, &this->lock);
  }
  pthread_mutex_unlock(&this->lock);

  // return the exit code
  int code = this->stopped;
  if (code != HPX_SUCCESS && here->rank == 0) {
    log_error("hpx_run epoch exited with a non-zero exit code: %d.\n", code);
  }
  return code;
}

void scheduler_stop(scheduler_t *this, int code) {
  pthread_mutex_lock(&this->lock);
  sync_store(&this->stopped, code, SYNC_RELEASE);
  this->state = SCHED_STOP;
  pthread_cond_broadcast(&this->running);
  pthread_mutex_unlock(&this->lock);
}

int scheduler_is_stopped(const scheduler_t *this) {
  int stopped = sync_load(&this->stopped, SYNC_ACQUIRE);
  return (stopped != SCHED_RUN);
}
