#!/bin/bash

CLASS=(
    normal
    stripall
    stripdebug
)

DISASSEMBLERS=(
    ghidra-9.1.2
    ghidra-10.2.3
    r2-4.4.0
    r2-5.8.4
    objdump
)

for c in ${CLASS[@]}; do
    for d in ${DISASSEMBLERS[@]}; do
        mkdir -p "static/$c/$d"
    done
done
