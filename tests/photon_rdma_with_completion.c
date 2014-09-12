#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <check.h>
#include "tests.h"
#include "photon.h"

#define PHOTON_SEND_SIZE 32
#define PHOTON_TAG       UINT32_MAX

//****************************************************************************
// This testcase tests RDMA with completion function
//****************************************************************************
START_TEST(test_rdma_with_completion) 
{
  int rank, size, i, next;
  int ret, flag, rc;
  int send_comp = 0;
  printf("Starting RDMA with completion test\n");

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  next = (rank+1) % size;

  struct photon_buffer_t rbuf;
  photon_rid sendReq, recvReq, req, request;
  char *send, *recv;

  send = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));
  recv = (char*)malloc(PHOTON_SEND_SIZE*sizeof(char));

  photon_register_buffer(send, PHOTON_SEND_SIZE);
  photon_register_buffer(recv, PHOTON_SEND_SIZE);

  printf("%d Sending buffer: ", rank);
  for (i = 0; i < PHOTON_SEND_SIZE; i++) {
    send[i] = i;
    printf("%d", send[i]);
  }
  printf("\n");


  // Post the recv buffer
  photon_post_recv_buffer_rdma(next, recv, PHOTON_SEND_SIZE, PHOTON_TAG, &recvReq);
  // Make sure we clear the local post event
  photon_wait_any(&ret, &request);
  // wait for a recv buffer that was posted
  photon_wait_recv_buffer_rdma(next, PHOTON_TAG, &sendReq);
  // Get the remote buffer info so we can do our own put.
  photon_get_buffer_remote(sendReq, &rbuf);


  // Put
  photon_put_with_completion(next, send, PHOTON_SEND_SIZE, (void*)rbuf.addr,
                               rbuf.priv, PHOTON_TAG, 0xcafebabe, 0);
  while (send_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &req, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (req == PHOTON_TAG)
        send_comp--;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);

  // Get
  send_comp = 0;
  photon_get_with_completion(next, send, PHOTON_SEND_SIZE, (void*)rbuf.addr, 
                             rbuf.priv, PHOTON_TAG, 0);
  while (send_comp) {
    rc = photon_probe_completion(PHOTON_ANY_SOURCE, &flag, &req, PHOTON_PROBE_ANY);
    if (rc != PHOTON_OK)
      continue;  // no events
    if (flag) {
      if (req == PHOTON_TAG)
        send_comp--;
    }
  }

  MPI_Barrier(MPI_COMM_WORLD);
  printf("%d received buffer: ", rank);
  int j;
  for (j = 0; j < PHOTON_SEND_SIZE; j++) {
    printf("%d", recv[j]);
  }
  printf("\n");

  photon_unregister_buffer(send, PHOTON_SEND_SIZE);
  photon_unregister_buffer(recv, PHOTON_SEND_SIZE);
  free(send);
  free(recv);

}
END_TEST

void add_photon_rdma_with_completion(TCase *tc) {
  tcase_add_test(tc, test_rdma_with_completion);
}
