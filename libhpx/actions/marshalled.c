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

#include <hpx/hpx.h>
#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include "init.h"

static void _pack_marshalled(const void *obj, hpx_parcel_t *p, int n,
                             va_list *args) {
}

static hpx_parcel_t *_new_marshalled(const void *obj, hpx_addr_t addr,
                                     hpx_addr_t c_addr, hpx_action_t c_action,
                                     int n, va_list *args) {
  dbg_assert_str(!n || args);
  dbg_assert(!n || n == 2);

  const action_t *action = obj;
  hpx_action_t id = *action->id;
  hpx_pid_t pid = hpx_thread_current_pid();
  void *data = (n) ? va_arg(*args, void*) : NULL;
  int bytes = (n) ? va_arg(*args, int) : 0;
  return parcel_new(addr, id, c_addr, c_action, pid, data, bytes);
}

static int _exec_marshalled(const void *obj, hpx_parcel_t *p) {
  const action_t *action = obj;
  hpx_action_handler_t handler = (hpx_action_handler_t)action->handler;
  void *args = hpx_parcel_get_data(p);
  return handler(args, p->size);
}

static int _exec_pinned_marshalled(const void *obj, hpx_parcel_t *p) {
  void *target;
  if (!hpx_gas_try_pin(p->target, &target)) {
    log_action("pinned action resend.\n");
    return HPX_RESEND;
  }

  const action_t *action = obj;
  hpx_pinned_action_handler_t handler =
      (hpx_pinned_action_handler_t)action->handler;
  void *args = hpx_parcel_get_data(p);
  return handler(target, args, p->size);
}

void action_init_marshalled(action_t *action) {
  uint32_t pinned = action->attr & HPX_PINNED;
  action->exec = (pinned) ? _exec_pinned_marshalled : _exec_marshalled;
  action->new = _new_marshalled;
  action->pack = _pack_marshalled;
}