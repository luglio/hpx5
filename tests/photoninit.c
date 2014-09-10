#include <check.h>
#include <stdio.h>                              /* FILE, fopen, sprintf, ... */
#include <inttypes.h>                           /* stdint formatting */
#include <check.h>
#include "photon.h"
#include "test_cfg.h"

/*
 --------------------------------------------------------------------
  TEST SUITE FIXTURE: library initialization
 --------------------------------------------------------------------
*/
void photontest_core_setup(void) {
  int rank, size;
  //char **forwarders = malloc(sizeof(char**));
  //forwarders[0] = "b001/5006";

  MPI_Comm_rank(MPI_COMM_WORLD,&rank);
  MPI_Comm_size(MPI_COMM_WORLD,&size);
  cfg.nproc = size;
  cfg.address = rank;
  //cfg.use_forwarder = 1;
  //cfg.forwarder_eids = forwarders;
  photon_init(&cfg);
}

/*
 --------------------------------------------------------------------
  TEST SUITE FIXTURE: library cleanup
 --------------------------------------------------------------------
*/
void photontest_core_teardown(void) {
  photon_finalize();
}

