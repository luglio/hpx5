#include "lulesh-hpx.h"

int get_bs_index(int i, int bi, int bs) {
  int r = hpx_get_num_ranks();
  //  printf("(%d, %d) -> %d\n", bi, i, (bi % r) + (bi/r)*bs*r + i*r);

  return (bi%r) + (bi/r)*bs*r + i*r;
}

// Performs the SBN1 update (allreduce)
int
_SBN1_result_action(NodalArgs *args) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain       *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // prepare for the unpack, do this here to minimize the time spent holding the
  // lock
  int srcLocalIdx = args->srcLocalIdx;
  double     *src = args->buf;
  int          nx = ld->sizeX + 1;
  int          ny = ld->sizeY + 1;
  int          nz = ld->sizeZ + 1;
  recv_t   unpack = RECEIVER[srcLocalIdx];

  // 1. acquire the domain lock
  //  hpx_lco_sema_p(ld->sem_sbn1);

  // static unsigned count[8] = {0};
  // printf("recv %u %u\n", ld->rank, count[ld->rank]++);
  // fflush(stdout);

  // 2. update the domain
  unpack(nx, ny, nz, src, ld->nodalMass, 0);

  // 3. release the domain lock
  //  hpx_lco_sema_v(ld->sem_sbn1);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->sbn1_and, HPX_NULL);

  hpx_gas_unpin(local);
  return HPX_SUCCESS;
}

/// Perform a single send operation for SBN1---basically just allocate and pack
/// a parcel. Exposed here as an action so that we can perform it in a parallel
/// spawn.
int
_SBN1_sends_action(pSBN *psbn)
{
  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  int destLocalIdx = psbn->destLocalIdx;
  hpx_parcel_t  *p = hpx_parcel_acquire(NULL, sizeof(NodalArgs) +
                                        BUFSZ[destLocalIdx]);
  assert(p);

  // interpret the parcel's buffer as a NodalArgs, and pack into its buffer
  NodalArgs *nodal = hpx_parcel_get_data(p);
  Domain   *domain = psbn->domain;
  int           nx = domain->sizeX + 1;
  int           ny = domain->sizeY + 1;
  int           nz = domain->sizeZ + 1;
  send_t      pack = SENDER[destLocalIdx];
  pack(nx, ny, nz, domain->nodalMass, nodal->buf);

  // compute the neighbor we are sending to (as byte offset in the domain)
  int    srcRemoteIdx = destLocalIdx;
  int     srcLocalIdx = 25 - srcRemoteIdx;
  int        distance = -OFFSET[srcLocalIdx];
  hpx_addr_t    local = hpx_thread_current_target();
  hpx_addr_t neighbor = hpx_addr_add(domain->base, sizeof(Domain) * (domain->rank + distance), sizeof(Domain));

  // pass along the source local index and epoch
  nodal->srcLocalIdx = srcLocalIdx;
  nodal->epoch = psbn->epoch;

  // static unsigned count[8] = {0};
  // printf("send %u %u\n", domain->rank, count[domain->rank]++);
  // fflush(stdout);

  // send the parcel--sync is fine because we packed in-place, we don't have a
  // continuation for this action since it's going to join the allreduce barrier
  // allocated at the remote domain, and we don't know it's global address here
  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _SBN1_result);
  hpx_parcel_send_sync(p);
  return HPX_SUCCESS;
}


/// Perform a parallel spawn of SBN1 messages for the domain and epoch
void
SBN1(hpx_addr_t local, Domain *domain, unsigned long epoch)
{
  const int    nsTF = domain->sendTF[0];
  const int *sendTF = &domain->sendTF[1];
  hpx_addr_t   done = hpx_lco_and_new(nsTF);

  int i;
  for (i = 0; i < nsTF; ++i) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(pSBN));
    assert(p);

    pSBN *psbn = hpx_parcel_get_data(p);

    psbn->domain       = domain;
    psbn->destLocalIdx = sendTF[i];
    psbn->epoch        = epoch;

    hpx_parcel_set_target(p, local);
    hpx_parcel_set_action(p, _SBN1_sends);
    hpx_parcel_set_cont_target(p, done);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);
    hpx_parcel_send(p, HPX_NULL);
  }

  // Make sure the parallel spawn loop above is complete before returning
  hpx_lco_wait(done);
  hpx_lco_delete(done, HPX_NULL);
}

int _SBN3_result_action(NodalArgs *args) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // prepare for the unpack, do this here to minimize the time spent holding the
  // lock
  int srcLocalIdx = args->srcLocalIdx;
  //  double *src = args->buf;

  int recvcnt = XFERCNT[srcLocalIdx];
  recv_t unpack = RECEIVER[srcLocalIdx];

  int nx = ld->sizeX + 1;
  int ny = ld->sizeY + 1;
  int nz = ld->sizeZ + 1;

  // 1. acquire the domain lock
  //  hpx_lco_sema_p(ld->sem_sbn3);

  // 2. update 
  //  unpack(nx, ny, nz, src, ld->fx, 0);
  //  unpack(nx, ny, nz, src + recvcnt, ld->fy, 0);
  //  unpack(nx, ny, nz, src + recvcnt*2, ld->fz, 0);

  // 3. release the domain lock
  //  hpx_lco_sema_v(ld->sem_sbn3);

  int gen = ld->cycle % 2;
  int fi = get_bs_index((srcLocalIdx + 26*(ld->rank))%26, ld->rank, 26);
  hpx_addr_t nodal_global;
  //  printf("Domain %d receiving from nf %d (%d, %d) in cycle %d\n", ld->rank, fi, ld->rank, (srcLocalIdx + 26*(ld->rank))%26, ld->cycle);
  printf("-1, %d, %d, %d, %d\n", ld->rank, ld->rank, srcLocalIdx, ld->cycle);
  //  nodal_global = hpx_lco_netfuture_getat(ld->sbn3[gen], fi, BUFSZ[srcLocalIdx] + sizeof(NodalArgs));
  int bi = ld->rank;
  size_t data_size = recvcnt * 3 * sizeof(double) + sizeof(NodalArgs);
  nodal_global = hpx_lco_netfuture_getat(ld->sbn3[gen][srcLocalIdx], bi, data_size);
  NodalArgs *nodal;
  bool pin_success = hpx_gas_try_pin(nodal_global, (void**)&nodal);
  assert(pin_success);
  double *src = nodal->buf;
  unpack(nx, ny, nz, src, ld->fx, 0);
  unpack(nx, ny, nz, src + recvcnt, ld->fy, 0);
  unpack(nx, ny, nz, src + recvcnt*2, ld->fz, 0);
  hpx_lco_netfuture_emptyat(ld->sbn3[gen][srcLocalIdx], bi, HPX_NULL);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->sbn3_and[args->epoch % 2], HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _SBN3_sends_action(pSBN *psbn)
{
  Domain *domain;
  domain = psbn->domain;
  hpx_addr_t local = hpx_thread_current_target();
  int destLocalIdx = psbn->destLocalIdx;

#if 0
  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(NodalArgs) +
				       BUFSZ[destLocalIdx]);
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  NodalArgs *nodal = hpx_parcel_get_data(p);
#endif
  NodalArgs *nodal;
  hpx_addr_t nodal_global = hpx_gas_alloc(XFERCNT[destLocalIdx]*3*sizeof(double) + sizeof(NodalArgs));
  bool pin_success = hpx_gas_try_pin(nodal_global, (void**)&nodal);
  assert(pin_success);

  send_t pack = SENDER[destLocalIdx];

  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int sendcnt = XFERCNT[destLocalIdx];
  pack(nx, ny, nz, domain->fx, nodal->buf);
  pack(nx, ny, nz, domain->fy, nodal->buf + sendcnt);
  pack(nx, ny, nz, domain->fz, nodal->buf + sendcnt*2);


  // the neighbor this is being sent to
  int srcRemoteIdx = destLocalIdx;
  int srcLocalIdx = 25 - srcRemoteIdx;
  int distance = -OFFSET[srcLocalIdx];
  hpx_addr_t neighbor = hpx_addr_add(domain->base, sizeof(Domain) * (domain->rank + distance), sizeof(Domain));

  // pass along the source local index and epoch
  nodal->srcLocalIdx = srcLocalIdx;
  nodal->epoch = psbn->epoch;
#if 0
  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _SBN3_result);
  hpx_parcel_send_sync(p);
#endif

  // pass along the source local index and epoch
  nodal->srcLocalIdx = srcLocalIdx;
  nodal->epoch = psbn->epoch;
  pack(nx, ny, nz, domain->fx, nodal->buf);
  pack(nx, ny, nz, domain->fy, nodal->buf + sendcnt);
  pack(nx, ny, nz, domain->fz, nodal->buf + sendcnt*2);

  int fi = get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26,
			domain->rank + distance, 26);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  int gen = domain->cycle % 2;
  // printf("Domain %d sending to nf %d (%d, %d) in cycle %d\n", domain->rank, fi, domain->rank + distance, (srcLocalIdx + 26*(domain->rank+distance))%26, domain->cycle);
  printf("1, %d, %d, %d, %d\n", domain->rank, domain->rank + distance, srcLocalIdx, domain->cycle);
  //  hpx_lco_netfuture_setat(domain->sbn3[gen], fi, BUFSZ[destLocalIdx] + sizeof(NodalArgs), nodal_global, lsync, HPX_NULL);
  int bi = domain->rank + distance;
  size_t data_size = sendcnt*3*sizeof(double) + sizeof(NodalArgs);
  hpx_lco_netfuture_setat(domain->sbn3[gen][srcLocalIdx], bi, 
			  data_size, nodal_global, 
			  lsync, HPX_NULL);
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);
  hpx_gas_free(nodal_global, HPX_NULL);

  return HPX_SUCCESS;
}

void SBN3(hpx_addr_t local,Domain *domain,unsigned long epoch)
{
  //  printf("sendrecv, domain, nfi, nf_b, nf_i, cycle\n");
  // pack outgoing data
  int nsTF = domain->sendTF[0];
  int *sendTF = &domain->sendTF[1];

  // for completing the parallel _SBN3_sends operations, so that we don't
  // release the lock too early
  hpx_addr_t sends = hpx_lco_and_new(nsTF);

  int i;
  for (i = 0; i < nsTF; i++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(pSBN));
    assert(p);

    pSBN *psbn = hpx_parcel_get_data(p);

    psbn->domain = domain;
    psbn->destLocalIdx = sendTF[i];
    psbn->epoch        = epoch;

    hpx_parcel_set_target(p, local);
    hpx_parcel_set_action(p, _SBN3_sends);
    hpx_parcel_set_cont_target(p, sends);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // async is fine, since we're waiting on sends below
    hpx_parcel_send(p, HPX_NULL);
  }

  // wait for incoming data
  int nrTF = domain->recvTF[0];
  int *recvTF = &domain->recvTF[1];

  hpx_addr_t src_addr;
  for (i = 0; i < nrTF; i++) {
    int srcLocalIdx = recvTF[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    NodalArgs args;
    args.srcLocalIdx = srcLocalIdx;
    args.epoch = epoch;
    hpx_call(local, _SBN3_result, &args, sizeof(args), HPX_NULL);
  }

  // Make sure the parallel spawn loop above is done so that we can release the
  // domain lock.
  hpx_lco_wait(sends);
  hpx_lco_delete(sends, HPX_NULL);
}

int _PosVel_result_action(NodalArgs *args) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // prepare for the unpack, do this here to minimize the time spent holding the
  // lock
  int srcLocalIdx = args->srcLocalIdx;

  int recvcnt = XFERCNT[srcLocalIdx];
  recv_t unpack = RECEIVER[srcLocalIdx];

  int nx = ld->sizeX + 1;
  int ny = ld->sizeY + 1;
  int nz = ld->sizeZ + 1;

  // 1. acquire the domain lock
  //  hpx_lco_sema_p(ld->sem_posvel);

  int gen = ld->cycle % 2;
  int fi = get_bs_index((srcLocalIdx + 26*ld->rank)%26, ld->rank, 26);
  hpx_addr_t nodal_global;
  int bi = ld->rank;
  size_t data_size = recvcnt*6*sizeof(double) + sizeof(NodalArgs);
  nodal_global = hpx_lco_netfuture_getat(ld->posvel[gen][srcLocalIdx], bi, data_size);

  NodalArgs *nodal;
  bool pin_success = hpx_gas_try_pin(nodal_global, (void**)&nodal);
  assert(pin_success);
  double *src = nodal->buf;

  // 2. update 
  unpack(nx, ny, nz, src, ld->x, 1);
  unpack(nx, ny, nz, src + recvcnt, ld->y, 1);
  unpack(nx, ny, nz, src + recvcnt*2, ld->z, 1);
  unpack(nx, ny, nz, src + recvcnt*3, ld->xd, 1);
  unpack(nx, ny, nz, src + recvcnt*4, ld->yd, 1);
  unpack(nx, ny, nz, src + recvcnt*5, ld->zd, 1);

  // 3. release the domain lock
  //  hpx_lco_sema_v(ld->sem_posvel);

  hpx_lco_netfuture_emptyat(ld->posvel[gen][srcLocalIdx], bi, HPX_NULL);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->posvel_and[args->epoch % 2], HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _PosVel_sends_action(pSBN *psbn)
{
  Domain *domain;
  domain = psbn->domain;
  hpx_addr_t local = hpx_thread_current_target();
  int destLocalIdx = psbn->destLocalIdx;

#if 0
  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(NodalArgs) +
                                       BUFSZ[destLocalIdx]);
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  NodalArgs *nodal = hpx_parcel_get_data(p);
#endif

  NodalArgs *nodal;
  //  hpx_addr_t nodal_global = hpx_gas_alloc(BUFSZ[destLocalIdx] + sizeof(NodalArgs));
  hpx_addr_t nodal_global = hpx_gas_alloc(XFERCNT[destLocalIdx]*6*sizeof(double) + sizeof(NodalArgs));
  bool pin_success = hpx_gas_try_pin(nodal_global, (void**)&nodal);
  assert(pin_success);

  send_t pack = SENDER[destLocalIdx];

  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int sendcnt = XFERCNT[destLocalIdx];

  pack(nx, ny, nz, domain->x, nodal->buf);
  pack(nx, ny, nz, domain->y, nodal->buf + sendcnt);
  pack(nx, ny, nz, domain->z, nodal->buf + sendcnt*2);
  pack(nx, ny, nz, domain->xd, nodal->buf + sendcnt*3);
  pack(nx, ny, nz, domain->yd, nodal->buf + sendcnt*4);
  pack(nx, ny, nz, domain->zd, nodal->buf + sendcnt*5);

  // the neighbor this is being sent to
  int srcRemoteIdx = destLocalIdx;
  int srcLocalIdx = 25 - srcRemoteIdx;
  int distance = -OFFSET[srcLocalIdx];
  hpx_addr_t neighbor = hpx_addr_add(domain->base, sizeof(Domain) * (domain->rank + distance), sizeof(Domain));

  // pass along the source local index and epoch
  nodal->srcLocalIdx = srcLocalIdx;
  nodal->epoch = psbn->epoch;
#if 0
  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _PosVel_result);
  hpx_parcel_send_sync(p);
#endif

  int fi = get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  int gen = domain->cycle % 2;
  int bi = domain->rank + distance;
  int data_size =  sendcnt*6*sizeof(double) + sizeof(NodalArgs);
  hpx_lco_netfuture_setat(domain->posvel[gen][srcLocalIdx], bi, 
			  data_size, nodal_global, 
			  lsync, HPX_NULL);
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);
  hpx_gas_free(nodal_global, HPX_NULL);

  return HPX_SUCCESS;
}

void PosVel(hpx_addr_t local,Domain *domain,unsigned long epoch)
{
  // doRecv = F, doSend = F, planeOnly = F, data = x, y, z, xd, yd, zd
  int nx = domain->sizeX + 1;
  int ny = domain->sizeY + 1;
  int nz = domain->sizeZ + 1;
  int tag = domain->cycle;
  int i;

  // pack outgoing data
  int nsFF = domain->sendFF[0];
  int *sendFF = &domain->sendFF[1];
  // for completing the PosVel_sends operations, so that we don't
  // release the lock too early
  hpx_addr_t sends = hpx_lco_and_new(nsFF);

  for (i = 0; i < nsFF; i++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(pSBN));
    assert(p);

    pSBN *psbn = hpx_parcel_get_data(p);

    psbn->domain = domain;
    psbn->destLocalIdx = sendFF[i];
    psbn->epoch        = epoch;

    hpx_parcel_set_target(p, local);
    hpx_parcel_set_action(p, _PosVel_sends);
    hpx_parcel_set_cont_target(p, sends);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // async is fine, since we're waiting on sends below
    hpx_parcel_send(p, HPX_NULL);
  }

  // wait for incoming data
  int nrFF = domain->recvFF[0];
  int *recvFF = &domain->recvFF[1];

  hpx_addr_t src_addr;
  for (i = 0; i < nrFF; i++) {
    int srcLocalIdx = recvFF[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    NodalArgs args;
    args.srcLocalIdx = srcLocalIdx;
    args.epoch = epoch;
    hpx_call(local, _PosVel_result, &args, sizeof(args), HPX_NULL);

  }

  // Make sure the parallel spawn loop above is done so that we can release the
  // domain lock.
  hpx_lco_wait(sends);
  hpx_lco_delete(sends, HPX_NULL);

}

int _MonoQ_result_action(NodalArgs *args) {
  hpx_addr_t local = hpx_thread_current_target();
  Domain *ld = NULL;

  if (!hpx_gas_try_pin(local, (void**)&ld))
    return HPX_RESEND;

  // prepare for the unpack, do this here to minimize the time spent holding the
  // lock
  int srcLocalIdx = args->srcLocalIdx;

  // move pointers to the ghost area
  double *delv_xi = ld->delv_xi;
  double *delv_eta = ld->delv_eta;
  double *delv_zeta = ld->delv_zeta;
  delv_xi += ld->numElem;
  delv_eta += ld->numElem;
  delv_zeta += ld->numElem;

  int nx = ld->sizeX;
  int planeElem = nx*nx;
  int data_size = planeElem * 3 * sizeof(double) + sizeof(NodalArgs);  

  // 2. update 
  int i = ld->reverse_recvTT[srcLocalIdx];

  int gen = ld->cycle % 2;
  int fi = get_bs_index((srcLocalIdx + 26*(ld->rank))%26, ld->rank, 26);
  hpx_addr_t nodal_global;
  int bi = ld->rank;
  nodal_global = hpx_lco_netfuture_getat(ld->monoq[gen][srcLocalIdx], bi, data_size);
  NodalArgs *nodal;
  bool pin_success = hpx_gas_try_pin(nodal_global, (void**)&nodal);
  assert(pin_success);
  double *src = nodal->buf;

  memcpy(delv_xi + i*planeElem, src, sizeof(double)*planeElem);
  memcpy(delv_eta + i*planeElem, src + planeElem, sizeof(double)*planeElem);
  memcpy(delv_zeta + i*planeElem, src + planeElem*2, sizeof(double)*planeElem);
  hpx_lco_netfuture_emptyat(ld->monoq[gen][srcLocalIdx], bi, HPX_NULL);

  // 4. join the and for this epoch---the _advanceDomain action is waiting on
  //    this before it performs local computation for the epoch
  hpx_lco_and_set(ld->monoq_and[args->epoch % 2], HPX_NULL);

  hpx_gas_unpin(local);

  return HPX_SUCCESS;
}

int _MonoQ_sends_action(pSBN *psbn)
{
  Domain *domain;
  domain = psbn->domain;
  hpx_addr_t local = hpx_thread_current_target();
  int destLocalIdx = psbn->destLocalIdx;

#if 0
  // Acquire a large-enough buffer to pack into.
  // - NULL first parameter means it comes with the parcel and is managed by
  //   the parcel and freed by the system inside of send()
  hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(NodalArgs) +
                                       BUFSZ[destLocalIdx]);
  assert(p);

  // "interpret the parcel buffer as a Nodal"
  NodalArgs *nodal = hpx_parcel_get_data(p);
#endif
  NodalArgs *nodal;
  send_t pack = SENDER[destLocalIdx];
 
  int nx = domain->sizeX;
  int ny = domain->sizeY;
  int nz = domain->sizeZ;

  double *delv_xi = domain->delv_xi;
  double *delv_eta = domain->delv_eta;
  double *delv_zeta = domain->delv_zeta;

  int numElem = domain->numElem;
  int planeElem = nx*nx;

  int data_size = planeElem * 3 * sizeof(double) + sizeof(NodalArgs);
  hpx_addr_t nodal_global = hpx_gas_alloc(data_size);
  bool pin_success = hpx_gas_try_pin(nodal_global, (void**)&nodal);
  assert(pin_success);

  pack(nx, ny, nz, delv_xi, nodal->buf);
  pack(nx, ny, nz, delv_eta, nodal->buf + planeElem);
  pack(nx, ny, nz, delv_zeta, nodal->buf + planeElem*2);

  // the neighbor this is being sent to
  int srcRemoteIdx = destLocalIdx;
  int srcLocalIdx = 25 - srcRemoteIdx;
  int distance = -OFFSET[srcLocalIdx];
  hpx_addr_t neighbor = hpx_addr_add(domain->base, sizeof(Domain) * (domain->rank + distance), sizeof(Domain));

  // pass along the source local index and epoch
  nodal->srcLocalIdx = srcLocalIdx;
  nodal->epoch = psbn->epoch;
#if 0
  hpx_parcel_set_target(p, neighbor);
  hpx_parcel_set_action(p, _MonoQ_result);
  hpx_parcel_send_sync(p);
#endif

  int fi = get_bs_index((srcLocalIdx + 26*(domain->rank+distance))%26, domain->rank+distance, 26);
  hpx_addr_t lsync = hpx_lco_future_new(0);
  int gen = domain->cycle % 2;
  int bi = domain->rank + distance;
  hpx_lco_netfuture_setat(domain->monoq[gen][srcLocalIdx], bi, 
			  data_size, nodal_global, 
			  lsync, HPX_NULL);
  hpx_lco_wait(lsync);
  hpx_lco_delete(lsync, HPX_NULL);
  hpx_gas_free(nodal_global, HPX_NULL);

  return HPX_SUCCESS;
}

void MonoQ(hpx_addr_t local,Domain *domain,unsigned long epoch)
{
  // pack outgoing data
  int nsTT = domain->sendTT[0];
  int *sendTT = &domain->sendTT[1];

  // for completing the parallel send operations, so that we don't
  // release the lock too early
  hpx_addr_t sends = hpx_lco_and_new(nsTT);

  int i; 
  for (i = 0; i < nsTT; i++) {
    hpx_parcel_t *p = hpx_parcel_acquire(NULL, sizeof(pSBN));
    assert(p);

    pSBN *psbn = hpx_parcel_get_data(p);

    psbn->domain = domain;
    psbn->destLocalIdx = sendTT[i];
    psbn->epoch        = epoch;

    hpx_parcel_set_target(p, local);
    hpx_parcel_set_action(p, _MonoQ_sends);
    hpx_parcel_set_cont_target(p, sends);
    hpx_parcel_set_cont_action(p, hpx_lco_set_action);

    // async is fine, since we're waiting on sends below
    hpx_parcel_send(p, HPX_NULL);
  }

  // wait for incoming data
  int nrTT = domain->recvTT[0];
  int *recvTT = &domain->recvTT[1];

  for (i = 0; i < nrTT; i++) {
    int srcLocalIdx = recvTT[i];
    int srcRemoteIdx = 25 - srcLocalIdx;
    NodalArgs args;
    args.srcLocalIdx = srcLocalIdx;
    args.epoch = epoch;
    hpx_call(local, _MonoQ_result, &args, sizeof(args), HPX_NULL);
  }

  // Make sure the parallel spawn loop above is done so that we can release the
  // domain lock.
  hpx_lco_wait(sends);
  hpx_lco_delete(sends, HPX_NULL);
}

void send1(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[0];
}

void send2(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < nx; i++)
    dest[i] = src[i];
}

void send3(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx - 1];
}

void send4(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < ny; i++)
    dest[i] = src[i*nx];
}

void send5(int nx, int ny, int nz, double *src, double *dest)
{
  memcpy(dest, src, nx*ny*sizeof(double));
}

void send6(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx - 1];
  for (i = 0; i < ny; i++)
    dest[i] = offsrc[i*nx];
}

void send7(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*(ny - 1)];
}

void send8(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1)];
  for (i = 0; i < nx; i++)
    dest[i] = offsrc[i];
}

void send9(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny - 1];
}

void send10(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < nz; i++)
    dest[i] = src[i*nx*ny];
}

void send11(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  for (i = 0; i < nz; i++)
    memcpy(&dest[i*nx], &src[i*nx*ny], nx*sizeof(double));
}

void send12(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx - 1];
  for (i = 0; i < nz; i++)
    dest[i] = offsrc[i*nx*ny];
}

void send13(int nx, int ny, int nz, double *src, double *dest)
{
  int i, j;
  for (i = 0; i < nz; i++) {
    double *offsrc = &src[i*nx*ny];
    for (j = 0; j < ny; j++)
      dest[i*ny + j] = offsrc[j*nx];
  }
}

void send14(int nx, int ny, int nz, double *src, double *dest)
{
  int i, j;
  for (i = 0; i < nz; i++) {
    double *offsrc = &src[i*nx*ny + nx - 1];
    for (j = 0; j < ny; j++)
      dest[i*ny + j] = offsrc[j*nx];
  }
}

void send15(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1)];
  for (i = 0; i < nz; i++)
    dest[i] = offsrc[i*nx*ny];
}

void send16(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1)];
  for (i = 0; i < nz; i++)
    memcpy(&dest[i*nx], &offsrc[i*nx*ny], nx*sizeof(double));
}

void send17(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny - 1];
  for (i = 0; i < nz; i++)
    dest[i] = offsrc[i*nx*ny];
}

void send18(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1)];
}

void send19(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1)];
  for (i = 0; i < nx; i++)
    dest[i] = offsrc[i];
}

void send20(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1) + nx - 1];
}

void send21(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1)];
  for (i = 0; i < ny; i++)
    dest[i] = offsrc[i*nx];
}

void send22(int nx, int ny, int nz, double *src, double *dest)
{
  double *offsrc = &src[nx*ny*(nz - 1)];
  memcpy(dest, offsrc, nx*ny*sizeof(double));
}

void send23(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*ny*(nz - 1) + nx - 1];
  for (i = 0; i < ny; i++)
    dest[i] = offsrc[i*nx];
}

void send24(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*(nz - 1) + nx*(ny - 1)];
}

void send25(int nx, int ny, int nz, double *src, double *dest)
{
  int i;
  double *offsrc = &src[nx*(ny - 1) + nx*ny*(nz - 1)];
  for (i = 0; i < nx; i++)
    dest[i] = offsrc[i];
}

void send26(int nx, int ny, int nz, double *src, double *dest)
{
  dest[0] = src[nx*ny*nz - 1];
}

void recv1(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[0] = src[0];
  } else {
    dest[0] += src[0];
  }
}

void recv2(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < nx; i++)
      dest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      dest[i] += src[i];
  }
}

void recv3(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx - 1] = src[0];
  } else {
    dest[nx - 1] += src[0];
  }
}

void recv4(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < ny; i++)
      dest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      dest[i*nx] += src[i];
  }
}

void recv5(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < nx*ny; i++)
      dest[i] = src[i];
  } else {
    for (i = 0; i < nx*ny; i++)
      dest[i] += src[i];
  }
}

void recv6(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx - 1];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      offdest[i*nx] += src[i];
  }
}
void recv7(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*(ny - 1)] = src[0];
  } else {
    dest[nx*(ny - 1)] += src[0];
  }
}

void recv8(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*(ny - 1)];

  if (type) {
    for (i = 0; i < nx; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      offdest[i] += src[i];
  }
}

void recv9(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny - 1] = src[0];
  } else {
    dest[nx*ny - 1] += src[0];
  }
}

void recv10(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  if (type) {
    for (i = 0; i < nz; i++)
      dest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      dest[i*nx*ny] += src[i];
  }
}

void recv11(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      for (j = 0; j < nx; j++)
    dest[i*nx*ny + j] = src[i*nx + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      for (j = 0; j < nx; j++)
    dest[i*nx*ny + j] += src[i*nx + j];
    }
  }
}

void recv12(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx - 1];

  if (type) {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] += src[i];
  }
}

void recv13(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny];
      for (j = 0; j < ny; j++)
    offdest[j*nx] = src[i*ny + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny];
      for (j = 0; j < ny; j++)
    offdest[j*nx] += src[i*ny + j];
    }
  }
}

void recv14(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny + nx - 1];
      for (j = 0; j < ny; j++)
    offdest[j*nx] = src[i*ny + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[i*nx*ny + nx - 1];
      for (j = 0; j < ny; j++)
    offdest[j*nx] += src[i*ny + j];
    }
  }
}

void recv15(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*(ny - 1)];

  if (type) {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] += src[i];
  }
}

void recv16(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i, j;
  if (type) {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[nx*(ny - 1) + i*nx*ny];
      for (j = 0; j < nx; j++)
    offdest[j] = src[i*nx + j];
    }
  } else {
    for (i = 0; i < nz; i++) {
      double *offdest = &dest[nx*(ny - 1) + i*nx*ny];
      for (j = 0; j < nx; j++)
    offdest[j] += src[i*nx + j];
    }
  }
}

void recv17(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny - 1];

  if (type) {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] = src[i];
  } else {
    for (i = 0; i < nz; i++)
      offdest[i*nx*ny] += src[i];
  }
}

void recv18(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1)] = src[0];
  } else {
    dest[nx*ny*(nz - 1)] += src[0];
  }
}

void recv19(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      offdest[i] += src[i];
  }
}

void recv20(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1) + nx - 1] = src[0];
  } else {
    dest[nx*ny*(nz - 1) + nx - 1] += src[0];
  }
}

void recv21(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      offdest[i*nx] += src[i];
  }
}

void recv22(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx*ny; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx*ny; i++)
      offdest[i] += src[i];
  }
}

void recv23(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*ny*(nz - 1) + nx - 1];

  if (type) {
    for (i = 0; i < ny; i++)
      offdest[i*nx] = src[i];
  } else {
    for (i = 0; i < ny; i++)
      offdest[i*nx] += src[i];
  }
}

void recv24(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*(nz - 1) + nx*(ny - 1)] = src[0];
  } else {
    dest[nx*ny*(nz - 1) + nx*(ny - 1)] += src[0];
  }
}

void recv25(int nx, int ny, int nz, double *src, double *dest, int type)
{
  int i;
  double *offdest = &dest[nx*(ny - 1) + nx*ny*(nz - 1)];

  if (type) {
    for (i = 0; i < nx; i++)
      offdest[i] = src[i];
  } else {
    for (i = 0; i < nx; i++)
      offdest[i] += src[i];
  }
}

void recv26(int nx, int ny, int nz, double *src, double *dest, int type)
{
  if (type) {
    dest[nx*ny*nz - 1] = src[0];
  } else {
    dest[nx*ny*nz - 1] += src[0];
  }
}
