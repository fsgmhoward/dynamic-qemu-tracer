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

NAMES :=
NAMES += execlog
NAMES += hotblocks
NAMES += hotpages
NAMES += howvec
NAMES += lockstep
NAMES += hwprofile
NAMES += cache
NAMES += drcov

SONAMES := $(addsuffix .so,$(addprefix lib,$(NAMES)))

# The main QEMU uses Glib extensively so it's perfectly fine to use it
# in plugins (which many example do).
CFLAGS = $(GLIB_CFLAGS)
CFLAGS += -fPIC -Wall $(filter -W%, $(QEMU_CFLAGS))
CFLAGS += $(if $(findstring no-psabi,$(QEMU_CFLAGS)),-Wpsabi)
CFLAGS += $(if $(CONFIG_DEBUG_TCG), -ggdb -O0)

all: $(SONAMES)

schema.capnp.o: schema.capnp.c++
	$(CXX) $(CFLAGS) --std=c++17 -c -O0 -g -o $@ $<

%.o: %.cpp
	$(CXX) $(CFLAGS) --std=c++17 -c -O0 -g -o $@ $<

lib%.so: %.o schema.capnp.o
	$(CXX) -shared -Wl,-soname,$@ -o $@ $^ $(LDLIBS) -lcapnp -lkj

clean:
	rm -f *.o *.so *.d
	rm -Rf .libs

.PHONY: all clean
