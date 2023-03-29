# -*- Mode: makefile -*-
#
# This Makefile example is fairly independent from the main makefile
# so users can take and adapt it for their build. We only really
# include config-host.mak so we don't have to repeat probing for
# cflags that the main configure has already done for us.
#

BUILD_DIR := $(CURDIR)/../qemu-7.2.0/build

include $(BUILD_DIR)/config-host.mak

VPATH += $(SRC_PATH)/contrib/plugins

NAMES := capnp-capture

SONAMES := $(addsuffix .so,$(addprefix lib,$(NAMES)))

# The main QEMU uses Glib extensively so it's perfectly fine to use it
# in plugins (which many example do).
CFLAGS = $(GLIB_CFLAGS)
CFLAGS += -fPIC -Wall $(filter -W%, $(QEMU_CFLAGS))
CFLAGS += $(if $(findstring no-psabi,$(QEMU_CFLAGS)),-Wpsabi)
CFLAGS += $(if $(CONFIG_DEBUG_TCG), -ggdb -O0)

all: $(SONAMES) print_result evaluator

print_result: print_result.cpp schema_io.cpp
	$(CXX) --std=c++17 -flto -O0 -g $^ -lcapnp -lkj -lZydis -o $@

fp-analyzer: fp-analyzer.cpp
	$(CXX) --std=c++17 -flto -O0 -g $^ -lsqlite3 -lcapnp -lkj -o $@

evaluator: evaluator.cpp schema_io.cpp
	$(CXX) --std=c++17 -flto -O0 -g $^ -lcapnp -lkj -o $@

schema.capnp.o: schema.capnp.c++
	$(CXX) $(CFLAGS) --std=c++17 -c -O3 -g -o $@ $<

%.o: %.cpp
	$(CXX) $(CFLAGS) --std=c++17 -c -O3 -g -o $@ $<
	
%.o: %.c
	$(CC) $(CFLAGS) -c -O3 -g -o $@ $<

lib%.so: %.o schema.capnp.o elf-parser.o schema_io.o
	$(CXX) -flto -shared -Wl,-soname,$@ -o $@ $^ $(LDLIBS) -lcapnp -lkj

clean:
	rm -f *.o *.so *.d
	rm -Rf .libs
	rm -rf evaluator fp-analyzer print_result

cleanall: clean
	rm -f *.out *.txt *.log

.PHONY: all clean
