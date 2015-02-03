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

/// @file libhpx/scheduler/lco.c
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include <libsync/sync.h>
#include "libhpx/action.h"
#include "libhpx/attach.h"
#include "libhpx/locality.h"
#include "libhpx/scheduler.h"
#include "libhpx/parcel.h"
#include "lco.h"
#include "thread.h"

/// We pack state into the LCO pointer---least-significant-bit is already used
/// in the sync_lockable_ptr interface
#define _TRIGGERED_MASK    (0x2)
#define _DELETED_MASK      (0x4)
#define _STATE_MASK        (0x7)

/// return the class pointer, masking out the state.
static const lco_class_t *_class(lco_t *lco) {
  const lco_class_t *class = sync_lockable_ptr_read(&lco->lock);
  uintptr_t bits = (uintptr_t)class;
  bits = bits & ~_STATE_MASK;
  return (lco_class_t*)bits;
}

static lco_t *_target_lco(void) {
  lco_t *lco = hpx_thread_current_local_target();
  dbg_assert_str(lco, "Could not pin LCO.");
  return lco;
}

static hpx_status_t _fini(lco_t *lco) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_fini, "LCO implementation incomplete\n");
  _class(lco)->on_fini(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _set(lco_t *lco, size_t size, const void *data) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_set, "LCO implementation incomplete\n");
  _class(lco)->on_set(lco, size, data);
  return HPX_SUCCESS;
}

static hpx_status_t _error(lco_t *lco, hpx_status_t code) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_error, "LCO implementation incomplete\n");
  _class(lco)->on_error(lco, code);
  return HPX_SUCCESS;
}

static hpx_status_t _reset(lco_t *lco) {
  _class(lco)->on_reset(lco);
  return HPX_SUCCESS;
}

static hpx_status_t _get(lco_t *lco, size_t bytes, void *out) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_get, "LCO implementation incomplete\n");
  return _class(lco)->on_get(lco, bytes, out);
}

static hpx_status_t _getref(lco_t *lco, size_t bytes, void **out) {
  return _class(lco)->on_getref(lco, bytes, out);
}

static hpx_status_t _release(lco_t *lco, void *out) {
  _class(lco)->on_release(lco, out);
  return HPX_SUCCESS;
}

static hpx_status_t _wait(lco_t *lco) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_wait, "LCO implementation incomplete\n");
  return _class(lco)->on_wait(lco);
}

static hpx_status_t _attach(lco_t *lco, hpx_parcel_t *p) {
  dbg_assert_str(_class(lco), "LCO vtable pointer is null\n");
  dbg_assert_str(_class(lco)->on_attach, "LCO implementation incomplete\n");
  return _class(lco)->on_attach(lco, p);
}

/// Action LCO event handler wrappers.
///
/// These try and pin the LCO, and then forward to the local event handler
/// wrappers. If the pin fails, then the LCO isn't local, so the parcel is
/// resent.
///
/// @{
static HPX_PINNED(_lco_fini, void *args) {
  return _fini(_target_lco());
}

HPX_PINNED(hpx_lco_set_action, void *data) {
  return _set(_target_lco(), hpx_thread_current_args_size(), data);
}

static HPX_PINNED(_lco_error, void *args) {
  hpx_status_t *code = args;
  return _error(_target_lco(), *code);
}

static HPX_PINNED(_lco_reset, void *UNUSED) {
  return _reset(_target_lco());
}

static HPX_PINNED(_lco_getref, void *args) {
  dbg_assert(args);
  int *n = args;
  lco_t *lco = _target_lco();
  // convert to wait if there's no buffer
  if (*n == 0) {
    return _wait(lco);
  }

  // otherwise continue the LCO buffer
  void *buffer;
  hpx_status_t status = _getref(lco, *n, &buffer);
  if (status == HPX_SUCCESS) {
    hpx_thread_continue(*n, buffer);
  }
  else {
    return status;
  }
}

static HPX_PINNED(_lco_wait, void *args) {
  return _wait(_target_lco());
}

HPX_PINNED(attach, void *args) {
  /// @todo: This parcel copy shouldn't be necessary. If we can retain the
  ///        parent parcel and free it appropriately, then we could just enqueue
  ///        the args directly.
  const hpx_parcel_t *p = args;
  hpx_parcel_t *parcel = hpx_parcel_acquire(NULL, p->size);
  assert(parcel_size(p) == parcel_size(parcel));
  dbg_assert_str(parcel, "could not allocate a parcel to attach\n");
  memcpy(parcel, p, parcel_size(p));
  return _attach(_target_lco(), parcel);
}
/// @}

/// LCO bit packing and manipulation
/// @{
void lco_lock(lco_t *lco) {
  sync_lockable_ptr_lock(&lco->lock);
  dbg_assert(self && self->current);
  struct ustack *stack = parcel_get_stack(self->current);
  dbg_assert(stack);
  dbg_assert(!stack->in_lco || stack->in_lco == _class(lco));
  stack->in_lco = _class(lco);
  log_lco("%p acquired lco %p\n", (void*)self->current, (void*)stack->in_lco);
}

void lco_unlock(lco_t *lco) {
  dbg_assert(self && self->current);
  struct ustack *stack = parcel_get_stack(self->current);
  log_lco("%p released lco %p\n", (void*)self->current, (void*)stack->in_lco);
  dbg_assert(stack);
  dbg_assert(stack->in_lco);
  dbg_assert_str(stack->in_lco == _class(lco), "lco %p in %p expected %p\n",
                 (void*)self->current, (void*)lco, (void*)_class(lco));
  stack->in_lco = NULL;
  sync_lockable_ptr_unlock(&lco->lock);
}

void lco_init(lco_t *lco, const lco_class_t *class) {
  lco->vtable = class;
}

void lco_fini(lco_t *lco) {
  DEBUG_IF(true) {
    lco->bits |= _DELETED_MASK;
  }
  lco_unlock(lco);
}

void lco_reset_deleted(lco_t *lco) {
  lco->bits &= ~_DELETED_MASK;
}

uintptr_t lco_get_deleted(const lco_t *lco) {
  return lco->bits & _DELETED_MASK;
}

void lco_set_triggered(lco_t *lco) {
  lco->bits |= _TRIGGERED_MASK;
}

void lco_reset_triggered(lco_t *lco) {
  lco->bits &= ~_TRIGGERED_MASK;
}

uintptr_t lco_get_triggered(const lco_t *lco) {
  return lco->bits & _TRIGGERED_MASK;
}

/// @}

void hpx_lco_delete(hpx_addr_t target, hpx_addr_t rsync) {
  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _fini(lco);
    hpx_gas_unpin(target);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call_async(target, _lco_fini, HPX_NULL, rsync, NULL, 0);
  dbg_check(e, "Could not forward lco_delete\n");
}

void hpx_lco_error(hpx_addr_t target, hpx_status_t code, hpx_addr_t rsync) {
  if (code == HPX_SUCCESS) {
    hpx_lco_set(target, 0, NULL, HPX_NULL, rsync);
    return;
  }

  if (target == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _error(lco, code);
    hpx_gas_unpin(target);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  size_t size = sizeof(code);
  int e = hpx_call_async(target, _lco_error, HPX_NULL, rsync, &code, size);
  dbg_check(e, "Could not forward lco_error\n");
}

void hpx_lco_reset(hpx_addr_t target, hpx_addr_t rsync) {
  if (target == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _reset(lco);
    hpx_gas_unpin(target);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call_async(target, _lco_reset, HPX_NULL, rsync, NULL, 0);
  dbg_check(e, "Could not forward lco_reset\n");
}

void hpx_lco_set(hpx_addr_t target, int size, const void *value,
                 hpx_addr_t lsync, hpx_addr_t rsync) {
  if (target == HPX_NULL) {
    return;
  }

  lco_t *lco = NULL;
  if ((size < HPX_LCO_SET_ASYNC) && hpx_gas_try_pin(target, (void**)&lco)) {
    _set(lco, size, value);
    hpx_gas_unpin(target);
    hpx_lco_set(lsync, 0, NULL, HPX_NULL, HPX_NULL);
    hpx_lco_set(rsync, 0, NULL, HPX_NULL, HPX_NULL);
    return;
  }

  int e = hpx_call_async(target, hpx_lco_set_action, lsync, rsync, value, size);
  dbg_check(e, "Could not forward lco_set\n");
}

hpx_status_t hpx_lco_wait(hpx_addr_t target) {
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    hpx_status_t status = _wait(lco);
    hpx_gas_unpin(target);
    return status;
  }

  return hpx_call_sync(target, _lco_wait, NULL, 0, NULL, 0);
}

/// If the LCO is local, then we use the local get functionality.
hpx_status_t hpx_lco_get(hpx_addr_t target, int size, void *value) {
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    dbg_assert(!size || value);
    dbg_assert(!value || size);
    hpx_status_t status = (size) ? _get(lco, size, value) : _wait(lco);
    hpx_gas_unpin(target);
    return status;
  }
  return hpx_call_sync(target, _lco_getref, value, size, &size, sizeof(size));
}

hpx_status_t hpx_lco_getref(hpx_addr_t target, int size, void **out) {
  dbg_assert(out && *out);
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    dbg_assert(!size || out);
    dbg_assert(!out || size);
    hpx_status_t status = (size) ? _getref(lco, size, out) : _wait(lco);
    hpx_gas_unpin(target);
    return status;
  }

  void *buffer = malloc(size); // ADK: can we get rid of this copy?
  assert(buffer);
  hpx_addr_t result = hpx_lco_future_new(size);
  int e = hpx_call(target, _lco_getref, result, &size, sizeof(size));
  if (e == HPX_SUCCESS) {
    e = hpx_lco_get(result, size, buffer);
    *out = buffer;
  }

  hpx_lco_delete(result, HPX_NULL);
  return e;
}

void hpx_lco_release(hpx_addr_t target, void *out) {
  lco_t *lco;
  if (hpx_gas_try_pin(target, (void**)&lco)) {
    _release(lco, out);
    hpx_gas_unpin(target);
  } else {
    // if the LCO is not local, delete the copied buffer.
    if (out) {
      free(out);
    }
  }
}

int hpx_lco_wait_all(int n, hpx_addr_t lcos[], hpx_status_t statuses[]) {
  dbg_assert(n > 0);

  // Will partition the lcos up into local and remote LCOs. We waste some stack
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address.
  lco_t *locals[n];
  hpx_addr_t remotes[n];

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote wait. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    if (!hpx_gas_try_pin(lcos[i], (void**)&locals[i])) {
      locals[i] = NULL;
      remotes[i] = hpx_lco_future_new(0);
      hpx_call_async(lcos[i], _lco_wait, HPX_NULL, remotes[i], NULL, 0);
    }
    else {
      remotes[i] = HPX_NULL;
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  int errors = 0;
  for (int i = 0; i < n; ++i) {
    hpx_status_t status = HPX_SUCCESS;
    if (locals[i] != NULL) {
      status = _wait(locals[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else {
      status = hpx_lco_wait(remotes[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
    if (status != HPX_SUCCESS) {
      ++errors;
    }
    if (statuses) {
      statuses[i] = status;
    }
  }
  return errors;
}

int hpx_lco_get_all(int n, hpx_addr_t lcos[], int sizes[], void *values[],
                    hpx_status_t statuses[]) {
  dbg_assert(n > 0);

  // Will partition the lcos up into local and remote LCOs. We waste some stack
  // space here, since, for each lco in lcos, we either have a local mapping or
  // a remote address.
  lco_t *locals[n];
  hpx_addr_t remotes[n];

  // Try and translate (and pin) all of the lcos, for any of the lcos that
  // aren't local, allocate a proxy future and initiate the remote get. This
  // two-phase approach achieves some parallelism.
  for (int i = 0; i < n; ++i) {
    if (!hpx_gas_try_pin(lcos[i], (void**)&locals[i])) {
      locals[i] = NULL;
      remotes[i] = hpx_lco_future_new(sizes[i]);
      hpx_call_async(lcos[i], _lco_getref, HPX_NULL, remotes[i], &sizes[i],
                     sizeof(sizes[i]));
    }
    else {
      remotes[i] = HPX_NULL;
    }
  }

  // Wait on all of the lcos sequentially. If the lco is local (i.e., we have a
  // local translation for it) we use the local get operation, otherwise we wait
  // for the completion of the remote proxy.
  int errors = 0;
  for (int i = 0; i < n; ++i) {
    hpx_status_t status = HPX_SUCCESS;
    if (locals[i] != NULL) {
      status = _get(locals[i], sizes[i], values[i]);
      hpx_gas_unpin(lcos[i]);
    }
    else {
      status = hpx_lco_get(remotes[i], sizes[i], values[i]);
      hpx_lco_delete(remotes[i], HPX_NULL);
    }
    if (status != HPX_SUCCESS) {
      ++errors;
    }
    if (statuses) {
      statuses[i] = status;
    }
  }
  return errors;
}
