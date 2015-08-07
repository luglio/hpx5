#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <string.h>
#include <limits.h>

#include "photon_backend.h"
#include "photon_buffer.h"
#include "photon_exchange.h"
#include "photon_event.h"

#include "photon_fi.h"
#include "photon_fi_connect.h"

#include "logging.h"
#include "libsync/locks.h"

struct rdma_args_t {
  int proc;
  uint64_t id;
  uint64_t laddr;
  uint64_t raddr;
  uint64_t size;
};

static int __initialized = 0;

static int fi_initialized(void);
static int fi_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss);
static int fi_finalize(void);
static int fi_get_info(ProcessInfo *pi, int proc, void **info, int *size, photon_info_t type);
static int fi_set_info(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type);
static int fi_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
		       photonBuffer lbuf, photonBuffer rbuf, uint64_t id, uint64_t imm_data,
		       int flags);
static int fi_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
		       photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags);
static int fi_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
			photonBuffer lbuf, uint64_t id, uint64_t imm_data, int flags);
static int fi_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
			photonBuffer lbuf, uint64_t id, int flags);
static int fi_get_event(int proc, int max, photon_rid *ids, int *n);
static int fi_get_revent(int proc, int max, photon_rid *ids, uint64_t *imms, int *n);

static fi_cnct_ctx fi_ctx = {
  .node = NULL,
  .service = NULL,
  .domain = NULL,
  .provider = NULL,
  .num_cq = 1,
  .rdma_put_align = PHOTON_FI_PUT_ALIGN,
  .rdma_get_align = PHOTON_FI_GET_ALIGN
};

/* we are now a Photon backend */
struct photon_backend_t photon_fi_backend = {
  .context = NULL,
  .initialized = fi_initialized,
  .init = fi_init,
  .cancel = NULL,
  .finalize = fi_finalize,
  .connect = NULL,
  .get_info = fi_get_info,
  .set_info = fi_set_info,
  /* API */
  .get_dev_addr = NULL,
  .register_addr = NULL,
  .unregister_addr = NULL,
  .register_buffer = NULL,
  .unregister_buffer = NULL,
  .test = NULL,
  .wait = NULL,
  .wait_ledger = NULL,
  .send = NULL,
  .recv = NULL,
  .post_recv_buffer_rdma = NULL,
  .post_send_buffer_rdma = NULL,
  .post_send_request_rdma = NULL,
  .wait_recv_buffer_rdma = NULL,
  .wait_send_buffer_rdma = NULL,
  .wait_send_request_rdma = NULL,
  .post_os_put = NULL,
  .post_os_get = NULL,
  .send_FIN = NULL,
  .probe_ledger = NULL,
  .probe = NULL,
  .wait_any = NULL,
  .wait_any_ledger = NULL,
  .io_init = NULL,
  .io_finalize = NULL,
  /* data movement */
  .rdma_put = fi_rdma_put,
  .rdma_get = fi_rdma_get,
  .rdma_send = fi_rdma_send,
  .rdma_recv = fi_rdma_recv,
  .get_event = fi_get_event,
  .get_revent = fi_get_revent
};

static int fi_initialized() {
  if (__initialized == 1)
    return PHOTON_OK;
  else
    return PHOTON_ERROR_NOINIT;
}

static int fi_init(photonConfig cfg, ProcessInfo *photon_processes, photonBI ss) {

  // __initialized: 0 - not; -1 - initializing; 1 - initialized
  __initialized = -1;
  
  fi_ctx.num_cq = cfg->cap.num_cq;

  fi_ctx.hints = fi_allocinfo();
  if (!fi_ctx.hints) {
    log_err("Could not allocate space for fi hints");
    goto error_exit;
  }

  //fi_ctx.hints->domain_attr->name = strdup("sockets");
  fi_ctx.hints->domain_attr->mr_mode = FI_MR_SCALABLE;
  fi_ctx.hints->domain_attr->threading = FI_THREAD_SAFE;
  fi_ctx.hints->ep_attr->type = FI_EP_RDM;
  fi_ctx.hints->caps = FI_MSG | FI_RMA | FI_RMA_EVENT;
  fi_ctx.hints->mode = FI_CONTEXT | FI_LOCAL_MR;

  if(__fi_init_context(&fi_ctx)) {
    log_err("Could not initialize libfabric context");
    goto error_exit;
  }

  fi_freeinfo(fi_ctx.hints);

  __initialized = 1;

  return PHOTON_OK;

error_exit:
  return PHOTON_ERROR;
}

static int fi_finalize() {
  return PHOTON_OK;
}

static int fi_get_info(ProcessInfo *pi, int proc, void **ret_info, int *ret_size, photon_info_t type) {
  return PHOTON_OK;
}

static int fi_set_info(ProcessInfo *pi, int proc, void *info, int size, photon_info_t type) {
  return PHOTON_OK;
}

static int fi_rdma_put(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
		       photonBuffer lbuf, photonBuffer rbuf, uint64_t id,
		       uint64_t imm_data, int flags) {
  return PHOTON_OK;
}

static int fi_rdma_get(int proc, uintptr_t laddr, uintptr_t raddr, uint64_t size,
		       photonBuffer lbuf, photonBuffer rbuf, uint64_t id, int flags) {
  return PHOTON_OK;
}

static int fi_rdma_send(photonAddr addr, uintptr_t laddr, uint64_t size,
			photonBuffer lbuf, uint64_t id, uint64_t imm_data, int flags) {
  return PHOTON_OK;
}

static int fi_rdma_recv(photonAddr addr, uintptr_t laddr, uint64_t size,
			photonBuffer lbuf, uint64_t id, int flags) {
  return PHOTON_OK;
}

static int fi_get_event(int proc, int max, photon_rid *ids, int *n) {
  return PHOTON_EVENT_OK;
}

static int fi_get_revent(int proc, int max, photon_rid *ids, uint64_t *imms, int *n) {
  return PHOTON_EVENT_OK;
}
