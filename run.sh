BINARY_PATH=$(realpath "${1}")
qemu-x86_64 -plugin /home/howard/qemu-tracer/libcapnp-capture.so,binary="${BINARY_PATH}",version=1 -d plugin $@
