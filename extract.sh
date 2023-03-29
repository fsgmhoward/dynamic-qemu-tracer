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

DYNAMIC_RESULTS=( $( find $INPATH -name '*.capnp.out' ) )

for dyn in ${DYNAMIC_RESULTS[@]}; do
    bin=${dyn%.capnp.out}
    bin_base=$(basename -- "${bin}")
    dir=$(dirname "${dyn}")
    cmd="${dir}/speccmds.out"
    
    cp "${dyn}" "${OUTDYN}"
    cp "${cmd}" "${OUTCMD}/${bin_base}.speccmds.out"
    cp "${bin}" "${OUTBIN}"
    cp "${bin}" "${OUTBIN_SALL}"
    cp "${bin}" "${OUTBIN_SDEBUG}"
done

strip -s "${OUTBIN_SALL}"/*
strip -d "${OUTBIN_SDEBUG}"/*
