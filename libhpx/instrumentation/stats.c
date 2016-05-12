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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>

#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/scheduler.h>
#include <hpx/hpx.h>
#include "metadata.h"

static void _vappend(int UNUSED, int n, int id, ...) {
  if (!self) {
    return;
  }

  int c = TRACE_EVENT_TO_CLASS[id];
  if (inst_trace_class(c)) {
    self->stats[id]++;
  }
}

static void _start(void) {
  for (int k = 0; k < HPX_THREADS; ++k) {
    worker_t *w = scheduler_get_worker(here->sched, k);
    w->stats = calloc(TRACE_NUM_EVENTS, sizeof(uint64_t));
  }
}

static void _destroy(void) {
  worker_t *master = scheduler_get_worker(here->sched, 0);
  for (int k = 1; k < HPX_THREADS; ++k) {
    worker_t *w = scheduler_get_worker(here->sched, k);
    for (int i = 0; i < TRACE_NUM_EVENTS; ++i) {
      int c = TRACE_EVENT_TO_CLASS[i];
      if (inst_trace_class(c)) {
        master->stats[i] += w->stats[i];
      }
    }
    free(w->stats);
  }

  for (int i = 0; i < TRACE_NUM_EVENTS; ++i) {
    int c = TRACE_EVENT_TO_CLASS[i];
    if (inst_trace_class(c)) {
      printf("%s,%lu\n", TRACE_EVENT_TO_STRING[i], master->stats[i]);
    }
  }
  free(master->stats);
}

trace_t *trace_stats_new(const config_t *cfg) {
  trace_t *trace = malloc(sizeof(*trace));
  dbg_assert(trace);

  trace->type    = HPX_TRACE_BACKEND_STATS;
  trace->start   = _start;
  trace->destroy = _destroy;
  trace->vappend = _vappend;
  return trace;
}