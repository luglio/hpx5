#ifndef PHOTON_H
#define PHOTON_H

#include <stdint.h>
#include <mpi.h>

struct photon_config_t {
  uint64_t address;
  int nproc;

  int use_cma;
  int use_forwarder;
  char **forwarder_eids;

  MPI_Comm comm;

  char *backend;
  int meta_exch;

  char *eth_dev;
  char *ib_dev;
  int ib_port;
};

struct photon_status_t {
  uint64_t src_addr;
  uint64_t size;
  int request;
  int tag;
  int count;
  int error;
};

struct photon_buffer_priv_t {
  uint64_t key0;
  uint64_t key1;
};

struct photon_descriptor_t {
  uintptr_t addr;
  uint64_t size;
  struct photon_buffer_priv_t priv;
};

typedef struct photon_config_t * photonConfig;
typedef struct photon_status_t * photonStatus;
typedef struct photon_buffer_priv_t * photonBufferPriv;
typedef struct photon_descriptor_t * photonDescriptor;

#define PHOTON_OK              0x0000
#define PHOTON_ERROR_NOINIT    0x0001
#define PHOTON_ERROR           0x0002

#define PHOTON_EXCH_TCP        0x0000
#define PHOTON_EXCH_MPI        0x0001
#define PHOTON_EXCH_XSP        0x0002

#define PHOTON_SEND_LEDGER     0x0000
#define PHOTON_RECV_LEDGER     0x0001

#define PHOTON_ANY_SOURCE      -1

int photon_initialized();
int photon_init(photonConfig cfg);
int photon_finalize();

int photon_register_buffer(void *buf, uint64_t size);
int photon_unregister_buffer(void *buf, uint64_t size);
int photon_get_buffer_private(void *buf, uint64_t size, photonBufferPriv ret_priv);

int photon_post_recv_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
int photon_post_send_buffer_rdma(int proc, void *ptr, uint64_t size, int tag, uint32_t *request);
int photon_post_send_request_rdma(int proc, uint64_t size, int tag, uint32_t *request);
int photon_wait_recv_buffer_rdma(int proc, int tag, uint32_t *oper);
int photon_wait_send_buffer_rdma(int proc, int tag, uint32_t *oper);
int photon_wait_send_request_rdma(int tag, uint43_t *request);
int photon_post_os_put(int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset, uint32_t oper, uint32_t *request);
int photon_post_os_get(int proc, void *ptr, uint64_t size, int tag, uint64_t r_offset, uint32_t oper, uint32_t *request);
int photon_post_os_get_direct(int proc, void *ptr, uint64_t size, int tag, photonDescriptor rbuf, uint32_t *request);
int photon_send_FIN(int proc, uint32_t oper);
int photon_test(uint32_t request, int *flag, int *type, photonStatus status);

int photon_wait(uint32_t request);
int photon_wait_ledger(uint32_t request);

int photon_wait_any(int *ret_proc, uint32_t *ret_req);
int photon_wait_any_ledger(int *ret_proc, uint32_t *ret_req);

int photon_probe_ledger(int proc, int *flag, int type, photonStatus status);

#endif
