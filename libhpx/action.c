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

#include <stdlib.h>
#include <string.h>

#include "libhpx/action.h"
#include "libhpx/debug.h"
#include "libhpx/utils.h"
#include "libsync/hashtables.h"

#if defined(ENABLE_DEBUG) || defined(ENABLE_ACTION_TABLE)
static cuckoo_hashtable_t _action_table = SYNC_CUCKOO_HASHTABLE_INIT;
#endif

#ifdef ENABLE_DEBUG
typedef struct {
  hpx_action_handler_t f;
  const char *key;
} _action_entry_t;
#endif


bool hpx_action_eq(const hpx_action_t lhs, const hpx_action_t rhs) {
  return (lhs == rhs);
}


int _dbg_action_insert(const long hkey, const hpx_action_handler_t f, const char *key) {
  _action_entry_t *entry = malloc(sizeof(*entry));
  entry->f = f;
  entry->key = key;
  return sync_cuckoo_hashtable_insert(&_action_table, (long)hkey, entry);
}

hpx_action_t action_register(const char *key, hpx_action_handler_t f) {
  int e;
#ifdef ENABLE_ACTION_TABLE
  size_t len = strlen(key);
  const long hkey = hpx_hash_string(key, len);
#if ENABLE_DEBUG
  e = _dbg_action_insert(hkey, f, key);
#else
  e = sync_cuckoo_hashtable_insert(&_action_table, hkey, (const void*)(hpx_action_t)f);
#endif
  assert(e);
  return (hpx_action_t)hkey;
#endif

#if ENABLE_DEBUG
  e = _dbg_action_insert((long)f, f, key);
  assert(e);
#endif
  return (hpx_action_t)f;
}


hpx_action_handler_t action_lookup(hpx_action_t id) {
#ifdef ENABLE_ACTION_TABLE
  const void *f = sync_cuckoo_hashtable_lookup(&_action_table, (long)id);
  assert(f);
#ifdef ENABLE_DEBUG
  return ((_action_entry_t*)f)->f;
#endif
  return (hpx_action_handler_t)(hpx_action_t)f;
#endif
  return (hpx_action_handler_t)id;
}


int action_invoke(hpx_action_t action, void *args) {
  hpx_action_handler_t handler = action_lookup(action);
  return handler(args);
}


const char *action_get_key(hpx_action_t id) {
  const char *key = NULL;
#ifdef ENABLE_DEBUG
  const _action_entry_t *entry = sync_cuckoo_hashtable_lookup(&_action_table, id);
  if (entry)
    key = entry->key;
#endif
  return key;
}


hpx_action_t
hpx_register_action(const char *id, hpx_action_handler_t func) {
  return action_register(id, func);
}
