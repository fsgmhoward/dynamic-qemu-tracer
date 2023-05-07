# Dynamic QEMU Tracer

This is a project of a QEMU TCG (Tiny Code Generator) plugin which tracks down file offsets of each instruction executed during runtime. 
It is designed to be used for disassembler benchmarks (to test the coverage and accuracy of the instructions recovered by static disassembler).

This could also be used as a form of "dynamic disassembly" but take note that it produces results only for instructions executed with the
given input.

## Compilation

Requirements:
- Ubuntu 22.04 OS (lower ones should still work)
- Installed build essentials (`apt install -y build-essential`) and other dependencies such as `cmake` and `ninja`
- Installed Cap'n Proto library (https://capnproto.org/install.html)
- Installed QEMU (either through package manager or compilation from the source code - the latter is recommended to
  ensure the same version of QEMU and the source code used to compile the plugin)
  
Steps:

(For QEMU source preperation)

1. Download QEMU sources (https://www.qemu.org/download/) and decompress the tar ball.
   Assuming the path is `~/qemu-7.2.0/`. (if not, modify `Makefile` line 9 to be your path of QEMU source code)
2. In decompressed source code directory, configure QEMU source code: `./configure --enable-lto --enable-avx2` (depend on your own needs). A directory called `build`
   should be created automatically for you.
3. If you have not installed QEMU through package manager, you may compile and install it: `make -j$(nproc) && sudo make install -j$(nproc)`.
   If you are just to compile the plugin, this is omittable.

(For compilation of this plugin)

1. Clone this project and store it somewhere. This document will use `~/qemu-tracer` as an example.
2. Modify `Makefile` and make sure variable `BUILD_DIR` matches with the location of your QEMU source code and the build directory
3. Execute `make` to build the project
4. After a successful build, there should be a dynamic library file called `libcapnp-capture.so` created, together with a number of auxillary programs for analysis.

(For conversion of the static disassembler result)

For the runner of the static disassemblers and the converter that converts the output to Cap'n Proto format, we reused the code from 
https://github.com/pangine/disasm-benchmark . Minor changes are made to the docker files to make it runable on the latest platform.

1. Install docker
2. Clone [This Repo](https://github.com/fsgmhoward/qemu-tracer-llvmmc-resolver) and set up according to the README.md
3. Clone [This Repo](https://github.com/fsgmhoward/qemu-tracer-eval-disasms) and set up according to the README.md

If nothing goes wrong, these will be the four docker images built to run static disassembly on given binaries:

```
pangine/ghidra-9.1.2
pangine/ghidra-10.2.3
pangine/r2-4.4.0
pangine/r2-5.8.4
```

## Usage

### Main Plugin

You may invoke the plugin on any program with such a command:

```
qemu-x86_64 -cpu max -plugin /full/path/to/libcapnp-capture.so,binary="/full/path/to/binary",version=1 -d plugin /full/path/to/binary
```

For example, you could capture instructions for `ls` command (with `/usr/local` as argument) by:

```
qemu-x86_64 -cpu max -plugin /full/path/to/libcapnp-capture.so,binary="$(which ls)",version=1 -d plugin $(which ls) /usr/local
```

Take note that value passed for `binary=` parameter needs to be a full path. Relative path is not acceptable. You may modify `run.sh` to change the 
plugin path, so you may run a program with this wrapper as simple as `./run.sh $(which ls) /usr/local`.

When a program is run in QEMU environment with this plugin enabled, it will create a file in this format:
```
<base_name_of_binary>.capnp.out
```

For example, when we capture `ls` command, it will create `ls.capnp.out` at the current working directory. This is the dynamic capture result which
stores the file offsets for each instructions run during the capture. If you would like to have a human-readable file for it, you could convert
it to plain-text using `capnp` utility which is installed together with the Cap'n Proto library:

```
capnp decode schema.capnp CaptureResult < ls.capnp.out > ls.capnp.txt
```

### Benchmarking Process (for SPEC2017)

Assuming SPEC2017 is installed at a location of `~/spec2017`.

1. Compile the relevant test programs with your own config. For example:
   ```
   # Delete all past build files
   runcpu --config=clang-llvm-linux-x86-fulllto.cfg --action=trash --tune=base --copies=1 all
   # Build new binaries and set up the running environment
   runcpu --config=clang-llvm-linux-x86-fulllto.cfg --action=runsetup --tune=base --copies=1 intrate
   ```
2. Modify `wrapper/wrapper.template` with the path to your previously built `libcapnp-capture.so`.
3. Use utility script to replace all binaries with the wrapper file, and run it (requires at least 9 threads for your VM/machine)
   ```
   cd ~/qemu-tracer/wrapper
   ./runall ~/spec2017
   ```
4. SPEC2017 ref inputs will be fed to the test programs and instructions executed will be captured.
5. After the execution (which will take a few hours or even a day, depends on the processor speed and optimization level), you can extract all
   capture files to one folder:
   ```
   cd ~/qemu-tracer
   ./extract.sh ~/spec2017 ~/output llvm-14-O3-fulllto
   ```
   And this will create a folder `~/output/llvm-14-O3-fulllto`, with binaries and dynamic capture results organized in it. It has such a
   directory format:
   ```
   .
    ├── binaries # unstripped binaries
    ├── binaries_stripall # strip -s
    ├── binaries_stripdebug # strip -d
    ├── cmds.out # specinvoke output, with time information
    ├── dynamic # dynamic capture results
    └── static # precreated folders for you to copy in static disassembler results, currently empty
   ```
6. Next, run static disassembler on one set of the binaries.
