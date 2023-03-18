qemu-x86_64 -plugin /home/howard/qemu-tracer/libcapnp-capture.so,binary=$1,version=1 -d plugin $BIN $@
