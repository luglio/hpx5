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
# include "config.h"
#endif

#include <stddef.h>
#include "libhpx/debug.h"
#include "libhpx/libhpx.h"
#include "libhpx/locality.h"
#include "libhpx/parcel.h"
#include "completions.h"
#include "eager_buffer.h"
#include "peer.h"
#include "../../gas/pgas/gpa.h"                 // sort of a hack

static hpx_action_t _eager_rx = 0;
static hpx_action_t _finish_eager_tx = 0;

static int _handle_eager_rx(void *UNUSED) {
  return LIBHPX_EUNIMPLEMENTED;
}

static int _handle_finish_eager_tx(void *UNUSED) {
  return LIBHPX_EUNIMPLEMENTED;
}

static HPX_CONSTRUCTOR void _init_handlers(void) {
  LIBHPX_REGISTER_INTERRUPT(&_eager_rx, _handle_eager_rx);
  LIBHPX_REGISTER_INTERRUPT(&_finish_eager_tx, _handle_finish_eager_tx);
}

static uint32_t _index_of(eager_buffer_t *buffer, uint64_t i) {
  return (i & (buffer->size - 1));
}

int eager_buffer_init(eager_buffer_t* b, peer_t *p, char *base, uint32_t size) {
  b->peer = p;
  b->size = size;
  b->min = 0;
  b->max = 0;
  b->base = base;
  return LIBHPX_OK;
}

void eager_buffer_fini(eager_buffer_t *b) {
}

int eager_buffer_tx(eager_buffer_t *buffer, hpx_parcel_t *p) {
  uint32_t n = parcel_network_size(p);
  if (n > buffer->size) {
    return dbg_error("cannot send %u byte parcels via eager put\n", n);
  }

  uint64_t end = buffer->max + n;
  if (end > (1ull << GPA_OFFSET_BITS)) {
    dbg_error("lifetime send buffer overflow handling unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  if (end - buffer->min > buffer->size) {
    dbg_log("could not send %u byte parcel\n", n);
    return LIBHPX_RETRY;
  }

  uint32_t roff = _index_of(buffer, buffer->max);
  uint32_t eoff = _index_of(buffer, end);
  int wrapped = (eoff <= roff);
  if (wrapped) {
    dbg_error("wrapped parcel send buffer handling unimplemented\n");
    return LIBHPX_EUNIMPLEMENTED;
  }

  buffer->max += n;

  const void *lva = parcel_network_offset(p);
  hpx_addr_t parcel = lva_to_gva(p);
  assert(parcel != HPX_NULL);
  completion_t lsync = encode_completion(_finish_eager_tx, parcel);
  completion_t completion = encode_completion(_eager_rx, buffer->max);
  return peer_pwc(buffer->peer, roff, lva, n, lsync, HPX_NULL, completion,
                  SEGMENT_EAGER);
}

hpx_parcel_t *eager_buffer_rx(eager_buffer_t *buffer) {
  return NULL;
}
