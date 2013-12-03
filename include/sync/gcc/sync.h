/*
  ====================================================================
  High Performance ParalleX Library (libhpx)

  Copyright (c) 2013, Trustees of Indiana University
  All rights reserved.

  This software may be modified and distributed under the terms of
  the BSD license.  See the COPYING file for details.

  This software was created at the Indiana University Center for
  Research in Extreme Scale Technologies (CREST).
  ====================================================================
*/

#pragma once
#ifndef HPX_SYNC_GCC_SYNC_H_
#define HPX_SYNC_GCC_SYNC_H_

/*
  ====================================================================
  Defines our synchronization interface in terms of the older gcc
  __sync builtins defined in
  http://gcc.gnu.org/onlinedocs/gcc/_005f_005fsync-Builtins.html#g_t_005f_005fsync-Builtins
  ====================================================================
*/

/* no memory model */
#define SYNC_RELAXED 0
#define SYNC_CONSUME 0
#define SYNC_ACQ_REL 0
#define SYNC_SEQ_CST 0
#define SYNC_ACQUIRE 0
#define SYNC_RELEASE 0

/*
 * I don't have a great way to implement an atomic load using macros, given the
 * interface that we're dealign with. We'll probably have to extend the
 * interface to take a type or something awkward...
 *
 * Not currently used in source, so we'll deal with it when we need it.
 */
#define sync_load(addr, mm) *addr

/*
 * Synchronizing a store requires that all previous operations complete before
 * the store occurs. Normal TSO (and x86-TSO) provides this in hardware (the
 * store won't bypass previous loads or stores), so we just need to make sure
 * that the compiler understands not to reorder the store with previous
 * operations.
 */
#define sync_store(addr, val, mm) do {          \
    __asm volatile ("":::"memory");             \
    *addr = val;                                \
  } while (0)

#define sync_swap(addr, val, mm) __sync_lock_test_and_set (addr, val)

#define sync_cas(addr, from, to, onsuccess, onfailure)  \
  __sync_bool_compare_and_swap(addr, from, to)

#define sync_cas_val(addr, from, to, onsuccess, onfailure)  \
  __sync_val_compare_and_swap(addr, from, to)

#define sync_fadd(addr, val, mm) __sync_fetch_and_add(addr, val)

#define sync_fence(mm) __sync_synchronize()

#define SYNC_ATOMIC(decl) volatile decl

/* ../generic.h implements all of the strongly-typed versions in
 * terms of the above generic versions.
 */
#include "../generic.h"

#endif /* HPX_SYNC_GCC_SYNC_H_ */
