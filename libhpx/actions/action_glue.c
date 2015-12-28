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

#include <libhpx/action.h>
#include <libhpx/debug.h>
#include <libhpx/parcel.h>
#include "init.h"

hpx_action_handler_t hpx_action_get_handler(hpx_action_t id) {
  CHECK_ACTION(id);
  const action_t *entry = &actions[id];
  return (hpx_action_handler_t)entry->handler;
}

int action_call_va(hpx_addr_t addr, hpx_action_t action, hpx_addr_t rsync,
                   hpx_action_t rop, hpx_addr_t lsync, hpx_addr_t gate,
                   int n, va_list *args) {
  hpx_parcel_t *p = action_new_parcel_va(action, addr, rsync, rop, n, args);

  if (likely(!gate && !lsync)) {
    parcel_launch(p);
    return HPX_SUCCESS;
  }
  if (!gate && lsync) {
    return hpx_parcel_send(p, lsync);
  }
  if (!lsync) {
    return hpx_parcel_send_through_sync(p, gate);
  }
  return hpx_parcel_send_through(p, gate, lsync);
}
