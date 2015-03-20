#!/bin/bash -x

OP=$1
DIR=$2
shift

if [ "$HPXMODE_AXIS" == smp ] ;
then
  export NUMNODES=1
else
  export NUMNODES=2
fi

function add_init() {
case "$SYSTEM" in
  CREST_cutter)
    . /opt/modules/Modules/3.2.10/init/bash
    module load openmpi/1.8.4_thread 
    ;;
  HPX5_BIGRED2)
    module load java
    module unload PrgEnv-cray
    module load PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
    export PATH=/N/home/h/p/hpx5/BigRed2/new_modules/bin:$PATH
    ;;
  HPX5_STAMPEDE)
    export LDFLAGS="-L/opt/ofed/lib64 -lpthread"
    export CPPFLAGS="-I/opt/ofed/include"
    ;;
  HPX5_EDISON)
    source /etc/profile.d/modules.sh
    module unload darshan
    module load atp
    module load git/2.0.0
    module swap PrgEnv-intel PrgEnv-gnu
    module load craype-hugepages8M
    export CRAYPE_LINK_TYPE=dynamic
    export PATH=/global/homes/j/jayaajay/autotools/bin:$PATH
    ;;
  *)
    echo "Unknown system $SYSTEM."
    exit 1
    ;;
esac

case "$BUILD_AXIS" in
  static)
    CFGFLAGS+=" --enable-static --disable-shared"
    ;;
  *)
    CFGFLAGS+=" --disable-static --enable-shared"
    ;;
esac
}

function add_mpi() {
case "$SYSTEM" in
  CREST_cutter)
    module load openmpi/1.8.4_thread
    CFGFLAGS+=" --with-mpi=ompi"
    ;;
  HPX5_BIGRED2)
    CFGFLAGS+=" --with-mpi"
    ;;
  HPX5_STAMPEDE)
    module load intel/13.1.1.163
    module load  impi/4.1.3.049
    CFGFLAGS+=" --with-mpi"
    ;;
  HPX5_EDISON)
    CFGFLAGS+=" --with-mpi"
    ;;
esac
}

function add_photon() {
CFGFLAGS+=" --enable-photon"
case "$SYSTEM" in
  CREST_cutter)
    export HPX_PHOTON_IBDEV=$HPXDEV_AXIS
    export HPX_PHOTON_BACKEND=verbs
    # verbs/rdmacm library not in jenkins node config
    export LD_LIBRARY_PATH=/usr/lib64:$LD_LIBRARY_PATH
    export LIBRARY_PATH=/usr/lib64:$LIBRARY_PATH
    ;;
  HPX5_BIGRED2)
    export HPX_PHOTON_BACKEND=ugni
    export HPX_PHOTON_CARGS="--with-ugni"
    CFGFLAGS+=" --with-pmi --with-hugetlbfs"
    ;;
  HPX5_STAMPEDE)
    export HPX_PHOTON_IBDEV=mlx4_0
    export HPX_PHOTON_BACKEND=verbs
    ;;
  HPX5_EDISON)
    export HPX_PHOTON_BACKEND=ugni
    export HPX_PHOTON_CARGS="--with-ugni"
    CFGFLAGS+=" --with-pmi --with-hugetlbfs"
    ;;
esac
}

function do_build() {
    echo "Building HPX in $DIR"
    cd "$DIR"
    
    echo "Bootstrapping HPX."
    git clean -xdf
    ./bootstrap
    
    if [ -d "./build" ]; then
        rm -rf ./build/
    fi
    mkdir build
    cd build
    
    if [ -d "./install" ]; then
        rm -rf ./install/
    fi
    mkdir install
    
    echo "Configuring HPX."
    eval "$CFG_CMD --prefix=${DIR}/build/install/ ${HPXDEBUG} ${CFGFLAGS} CFLAGS=\"-O3 -g\""  
  
    echo "Building HPX."
    make -j 8
    make install
}

CFGFLAGS+=" --enable-testsuite --enable-parallel-config"

add_init

case "$HPXMODE_AXIS" in
    photon)
	add_mpi
        add_photon
	;;
    mpi)
	add_mpi	
	;;
    *)
	;;
esac

case "$SYSTEM" in
  CREST_cutter)
    case "$HPXIBDEV" in
      qib0)
          export PSM_MEMORY=large
          CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --mca btl_openib_if_include ${HPXIBDEV}\""
          ;;
      mlx4_0)
          CFGFLAGS+=" --with-tests-cmd=\"mpirun -np 2 --mca mtl ^psm --mca btl_openib_if_include ${HPXIBDEV}\""
          ;;
      none)
          ;;
      *)
          ;;
    esac
  ;;
  HPX5_BIGRED2)
    if [ "$HPXMODE_AXIS" == smp ] ; then
      CFGFLAGS+=" --with-tests-cmd=\"aprun -n 1 -N 1\""
    else
      CFGFLAGS+=" --with-tests-cmd=\"aprun -n 2 -N 1\""
    fi
    ;;
  HPX5_STAMPEDE)
    if [ "$HPXMODE_AXIS" == smp ] ; then
      CFGFLAGS+=" --with-tests-cmd=\"ibrun -np 1\""
    else
      CFGFLAGS+=" --with-tests-cmd=\"ibrun -np 2\""
    fi
   ;;
  HPX5_EDISON)
    if [ "$HPXMODE_AXIS" == smp ] ; then
      CFGFLAGS+=" --with-tests-cmd=\"aprun -n 1 -N 1\""
    else
      CFGFLAGS+=" --with-tests-cmd=\"aprun -n 2 -N 1\""
    fi
    ;;
  *)
    exit 1
    ;;
esac

case "$SYSTEM" in
  CREST_cutter)
    case "$HPXCC" in
      gcc-4.6.4)
          CFGFLAGS+=" CC=gcc"
          ;;
      gcc-4.9.2)
          module load gcc/4.9.2
          CFGFLAGS+=" CC=gcc"
          ;;
      clang)
          module load llvm/3.6.0 
          CFGFLAGS+=" CC=clang"
          ;;
      *)
          ;;
    esac  
    ;; 
  HPX5_BIGRED2)
    CFGFLAGS+=" CC=cc"
    ;;
  HPX5_STAMPEDE)
    CFGFLAGS+=" CC=icc"
    ;;
  HPX5_EDISON)
    CFGFLAGS+=" CC=cc"
    ;;
  *)
    exit 1
    ;;
esac

case "$HPXDEBUG_CHOICE" in
  on)
    HPXDEBUG="--enable-debug"
    ;;
  off)
    HPXDEBUG="--disable-debug"
    ;;
esac

if [ "$OP" == "build" ]; then
    case "$SYSTEM" in
      CREST_cutter)
        CFG_CMD="../configure"
        ;;
      HPX5_BIGRED2)
        CFG_CMD="../configure"
        ;;
      HPX5_STAMPEDE)
        CFG_CMD=" MPI_CFLAGS=-I/opt/apps/intel13/impi/4.1.3.049/intel64/include MPI_LIBS=\"-L/opt/apps/intel13/impi/4.1.3.049/intel64/lib -lmpi\" ../configure"
        ;; 
      HPX5_EDISON)
        CFG_CMD="../configure"
        ;; 
      *)
       exit 1
       ;;
   esac
   do_build
fi

if [ "$OP" == "run" ]; then
    cd "$DIR/build"
   
    case "$SYSTEM" in	 
      HPX5_STAMPEDE)
      module load intel/13.1.1.163
      module load  impi/4.1.3.049
      ;;
    esac

    # Run all the unit tests:
    make check -C tests

    # Check the output of the unit tests:
    if grep '^# FAIL: *0$' $DIR/build/tests/unit/test-suite.log
     then
       echo "FAIL: 0"
     else
       cat $DIR/build/tests/unit/test-suite.log
       exit 1
    fi

    if egrep -q "(ERROR:)\s+[1-9][0-9]*" $DIR/build/tests/unit/test-suite.log
      then
        cat $DIR/build/tests/unit/test-suite.log
        exit 1
    fi
fi

exit 0
