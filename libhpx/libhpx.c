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

#include <signal.h>
#include <libhpx/debug.h>
#include <libhpx/libhpx.h>
#include <libhpx/locality.h>
#include <libhpx/scheduler.h>
#include <libhpx/system.h>

const libhpx_config_t *libhpx_get_config(void) {
  dbg_assert_str(here, "libhpx not initialized\n");
  dbg_assert_str(here->config, "libhpx config not available yet\n");
  return here->config;
}

/// Infrastructure for detecting incorrect usage inside of lightweight threads.
/// @{
int __real_sigprocmask(int, const sigset_t *, sigset_t *);
int __wrap_sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
  if (here && self && self->current) {
    dbg_error("Changing the signal mask during hpx_run is unsupported\n");
  }
  if (here && (!self || !self->current)) {
    log_dflt("Changing the signal mask between `hpx_run` epochs is "
             "supported however it must be restored before re-entering "
             "HPX execution.\n");
  }
  return __real_sigprocmask(how, set, oldset);
}

int __real_pthread_sigmask(int, const sigset_t *, sigset_t *);
int __wrap_pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
  if (here && self && self->current) {
    dbg_error("Changing the signal mask during hpx_run is unsupported.\n");
  }
  if (here && (!self || !self->current)) {
    log_dflt("Changing the signal mask between `hpx_run` epochs is "
             "supported however it must be restored before re-entering "
             "HPX execution.\n");
  }
  return __real_pthread_sigmask(how, set, oldset);
}

sighandler_t __real_signal(int, sighandler_t);
sighandler_t __wrap_signal(int signum, sighandler_t handler) {
  if (here) {
    dbg_error("Registering signal handlers after hpx_init is unsupported.\n");
  }
  return __real_signal(signum, handler);
}

int __real_sigaction(int, const struct sigaction *, struct sigaction *);
int __wrap_sigaction(int signum, const struct sigaction *act,
                     struct sigaction *oldact) {
  if (here) {
    dbg_error("Registering signal handlers after hpx_init is unsupported.\n");
  }
  return __real_sigaction(signum, act, oldact);
}

/// @}
