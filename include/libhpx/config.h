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
#ifndef LIBHPX_CONFIG_H
#define LIBHPX_CONFIG_H

/// @file
/// @brief Types and constants needed for configuring HPX at run-time.

//! Configuration options for which global memory model to use.
typedef enum {
  HPX_GAS_DEFAULT = 0, //!< Let HPX choose what memory model to use.
  HPX_GAS_SMP,         //!< Do not use global memory.
  HPX_GAS_PGAS,        //!< Use PGAS (i.e. global memory is fixed).
  HPX_GAS_AGAS,        //!< Use AGAS (i.e. global memory may move).
  HPX_GAS_PGAS_SWITCH, //!< Use hardware-accelerated PGAS.
  HPX_GAS_AGAS_SWITCH, //!< Use hardware-accelerated AGAS.
  HPX_GAS_MAX
} hpx_gas_t;

static const char* const HPX_GAS_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "PGAS",
  "AGAS",
  "PGAS_SWITCH",
  "AGAS_SWITCH",
  ""
};


//! Configuration options for the network transports HPX can use.
typedef enum {
  HPX_TRANSPORT_DEFAULT = 0, //!< Let HPX choose what transport to use.
  HPX_TRANSPORT_SMP,         //!< Do not use a network transport.
  HPX_TRANSPORT_MPI,         //!< Use MPI for network transport.
  HPX_TRANSPORT_PORTALS,     //!< Use Portals for network transport.
  HPX_TRANSPORT_PHOTON,      //!< Use Photon for network transport.
  HPX_TRANSPORT_MAX
} hpx_transport_t;

static const char* const HPX_TRANSPORT_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "MPI",
  "PORTALS",
  "PHOTON",
  ""
};


//! Configuration options for which bootstrappers HPX can use.
typedef enum {
  HPX_BOOT_DEFAULT = 0,      //!< Let HPX choose what bootstrapper to use.
  HPX_BOOT_SMP,              //!< Use the SMP bootstrapper.
  HPX_BOOT_MPI,              //!< Use mpirun to bootstrap HPX.
  HPX_BOOT_PMI,              //!< Use the PMI bootstrapper.
  HPX_BOOT_MAX
} hpx_boot_t;

static const char* const HPX_BOOT_TO_STRING[] = {
  "DEFAULT",
  "SMP",
  "MPI",
  "PMI",
  ""
};


//! Locality types in HPX.
typedef enum {
  HPX_LOCALITY_NONE = -2,    //!< Represents no locality.
  HPX_LOCALITY_ALL = -1      //!< Represents all localities.
} hpx_locality_t;

static const char* const HPX_LOCALITY_TO_STRING[] = {
  "NONE",
  "ALL"
};


//! Configuration options for runtime logging in HPX.
typedef enum {
  HPX_LOG_DEFAULT = (1<<0),  //!< The default logging level.
  HPX_LOG_BOOT    = (1<<1),  //!< Log the bootstrapper execution.
  HPX_LOG_SCHED   = (1<<2),  //!< Log the HPX scheduler operations.
  HPX_LOG_GAS     = (1<<3),  //!< Log the Global-Address-Space ops.
  HPX_LOG_LCO     = (1<<4),  //!< Log the LCO operations.
  HPX_LOG_NET     = (1<<5),  //!< Turn on logging for network ops.
  HPX_LOG_TRANS   = (1<<6),  //!< Log the transport operations.
  HPX_LOG_PARCEL  = (1<<7),  //!< Parcel logging.
  HPX_LOG_ALL     =   (-1)   //!< Turn on all logging.
} hpx_log_t;

static const char* const HPX_LOG_TO_STRING[] = {
  "LOG_DEFAULT",
  "LOG_BOOT",
  "LOG_SCHED",
  "LOG_GAS",
  "LOG_LCO",
  "LOG_NET",
  "LOG_TRANS",
  "LOG_PARCEL",
  "LOG_ALL"
};


// ----------------------------------------------------------------------------
/// The HPX configuration type (to give hpx_init()).
// ----------------------------------------------------------------------------
/// This configuration can be passed to hpx_init() to control some
/// runtime parameters for the HPX system.

typedef struct {
  int                 cores;          //!< number of cores to run on.
  int               threads;          //!< number of HPX scheduler threads.
  unsigned int   backoffmax;          //!< upper bound for backoff.
  int             stacksize;          //!< minimum stack size in bytes.
  size_t           heapsize;          //!< shared heap size in bytes
  hpx_gas_t             gas;          //!< Type of GAS.
  hpx_boot_t           boot;          //!< bootstrap method to use.
  hpx_transport_t transport;          //!< transport to use.
  hpx_locality_t     waitat;          //!< locality to wait on.
  hpx_log_t        loglevel;          //!< the logging level to use.
  int            statistics;          //!< print statistics.
  uint32_t         reqlimit;          //!< max transport requests
} hpx_config_t;

/// The default configuration parameters HPX will start with.
#define HPX_CONFIG_DEFAULTS {                   \
    .cores       = 0,                           \
    .threads     = 0,                           \
    .backoffmax  = 1024,                        \
    .stacksize   = 32768,                       \
    .heapsize    = 1lu << 30, /* 1GB */         \
    .gas         = HPX_GAS_PGAS,                \
    .boot        = HPX_BOOT_DEFAULT,            \
    .transport   = HPX_TRANSPORT_DEFAULT,       \
    .waitat      = HPX_LOCALITY_NONE,           \
    .loglevel    = HPX_LOG_DEFAULT,             \
    .statistics  = true,                        \
    .reqlimit    = 0                            \
  }

const char* hpx_get_network_id(void);

#endif // LIBHPX_CONFIG_H
