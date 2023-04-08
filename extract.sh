#!/bin/bash
# This script extracts compiled binary, dynamic capture result and speccmds.out from SPEC2017 folder

if [ "$#" -ne 3 ]; then
    echo "Invalid number of arguments"
    echo "./extract.sh <spec2017_dir> <output_dir> <dataset_name>"
    exit
fi

if [ ! -d "$1" ] || [ ! -f "$1/shrc" ]; then
    echo "Invalid spec2017 directory"
    echo "It needs to be the directory with 'shrc' file inside"
fi

INPATH="${1}/benchspec/CPU"
OUTPATH="${2}/${3}"

echo "INPATH=${INPATH}"
echo "OUTPATH=${OUTPATH}"

OUTBIN="${OUTPATH}/binaries"
OUTBIN_SDEBUG="${OUTPATH}/binaries_stripdebug"
OUTBIN_SALL="${OUTPATH}/binaries_stripall"
OUTCMD="${OUTPATH}/cmds.out"
OUTDYN="${OUTPATH}/dynamic"

mkdir -p "${OUTBIN}"
mkdir -p "${OUTBIN_SDEBUG}"
mkdir -p "${OUTBIN_SALL}"
mkdir -p "${OUTCMD}"
mkdir -p "${OUTDYN}"

PROGRAMS=(
  500.perlbench_r
  502.gcc_r
  505.mcf_r
  520.omnetpp_r
  523.xalancbmk_r
  525.x264_r
  531.deepsjeng_r
  541.leela_r
  557.xz_r
)

BINARIES=(
  perlbench_r_base.mytest-m64.orig
  cpugcc_r_base.mytest-m64.orig
  mcf_r_base.mytest-m64.orig
  omnetpp_r_base.mytest-m64.orig
  cpuxalan_r_base.mytest-m64.orig
  x264_r_base.mytest-m64.orig
  deepsjeng_r_base.mytest-m64.orig
  leela_r_base.mytest-m64.orig
  xz_r_base.mytest-m64.orig
)

DYNAMIC_RESULTS=( $( find $INPATH -name '*.capnp.out' ) )

for i in {0..8}; do
    PROGRAM="${PROGRAMS[$i]}"
    PROGPATH="${INPATH}/${PROGRAM}/run/run_base_refrate_mytest-m64.0000"
    
    bin="${BINARIES[$i]}"
    dyn="${bin}.capnp.out"
    cmd="speccmds.out"
    
    cp "${PROGPATH}/${dyn}" "${OUTDYN}"
    cp "${PROGPATH}/${cmd}" "${OUTCMD}/${bin}.speccmds.out"
    cp "${PROGPATH}/${bin}" "${OUTBIN}"
    cp "${PROGPATH}/${bin}" "${OUTBIN_SALL}"
    cp "${PROGPATH}/${bin}" "${OUTBIN_SDEBUG}"
done

strip -s "${OUTBIN_SALL}"/*
strip -d "${OUTBIN_SDEBUG}"/*
