#ifndef LIBPHOTON_H
#define LIBPHOTON_H

#include <stdint.h>
#include <pthread.h>
#include "config.h"
#include "photon.h"
#include "photon_io.h"

/* forward declarations */
typedef struct photon_backend_t   * photonBackend;
typedef struct photon_forwarder_t * photonForwarder;

/* Global config for the library */
extern photonConfig    __photon_config;
extern photonBackend   __photon_backend;
extern photonForwarder __photon_forwarder;

extern int _LEDGER_SIZE;
extern int _photon_myrank;
extern int _photon_nproc;
extern int _photon_nforw;
extern int _photon_fproc;
extern int _photon_ebsize;
extern int _photon_smsize;
extern int _photon_spsize;
extern int _photon_upsize;
extern int _forwarder;

#ifdef ENABLE_DEBUG
/* defined in util.c */
PHOTON_INTERNAL time_t _tictoc(time_t stime, int proc);
#endif

#endif
