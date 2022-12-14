LIBNAME := libsupla

CC ?= gcc
RM = rm -rf

NOSSL ?= 0

LIBVERSION ?= $(shell cut -d'"' -f2 include/libsupla/version.h)
LIB_SOVERSION = $(word 1,$(subst ., ,$(LIBVERSION)))

SRCS += src/supla-common/cfg.c
SRCS += src/supla-common/eh.c
SRCS += src/supla-common/ini.c
SRCS += src/supla-common/lck.c
SRCS += src/supla-common/log.c
SRCS += src/supla-common/proto.c
SRCS += src/supla-common/srpc.c
SRCS += src/supla-common/tools.c
SRCS += src/supla-common/supla-socket.c

SRCS += src/device.c
SRCS += src/channel.c
SRCS += src/supla-value.c
SRCS += src/supla-extvalue.c
SRCS += src/supla-action-trigger.c
#FIXME arch dependent
SRCS += src/port/arch_unix.c

OBJS = $(SRCS:.c=.o)

EXTRAINCDIRS= include
SUPLACOREINCDIR=include/libsupla/supla-common

LIB_SO_LDFLAG=-Wl,-soname=$(LIBNAME).so.$(LIB_SOVERSION)

PREFIX ?= /usr
INCLUDE_PATH ?= include
LIBRARY_PATH ?= lib

INSTALL_INCLUDE_PATH = $(DESTDIR)$(PREFIX)/$(INCLUDE_PATH)
INSTALL_LIBRARY_PATH = $(DESTDIR)$(PREFIX)/$(LIBRARY_PATH)

# library names
LIB_SHARED = $(LIBNAME).so
LIB_SHARED_VERSION = $(LIBNAME).so.$(LIBVERSION)
LIB_SHARED_SO = $(LIBNAME).so.$(LIB_SOVERSION)
LIB_STATIC = $(LIBNAME).a

CFLAGS = -fPIC -Wall -O2 -g
#-Wextra
ifeq ($(NOSSL), 1)
	CFLAGS += -DNOSSL
endif

CFLAGS += $(patsubst %,-I%,$(EXTRAINCDIRS))
LDFLAGS = 

.PHONY: all shared static clean install uninstall example

all: shared static

#static library
$(LIB_STATIC): $(OBJS)
	$(AR) rcs $@ $^

#shared library .so.1.0.0
$(LIB_SHARED_VERSION): $(OBJS)
	$(CC) -shared -o $@ $^ $(LIB_SO_LDFLAG) $(LDFLAGS)

#links .so -> .so.1 -> .so.1.0.0
$(LIB_SHARED_SO): $(LIB_SHARED_VERSION)
	ln -sf $(LIB_SHARED_VERSION) $(LIB_SHARED_SO)
$(LIB_SHARED): $(LIB_SHARED_SO)
	ln -sf $(LIB_SHARED_SO) $(LIB_SHARED)

#$(SRCS:.c=.d):%.d:%.c
#	$(CC) $(CFLAGS) -MM $< >$@

#include $(SRCS:.c=.d)

includes:
	@mkdir -p $(SUPLACOREINCDIR)
	@cp src/supla-common/*.h $(SUPLACOREINCDIR)

shared: includes $(LIB_SHARED)

static: includes $(LIB_STATIC)

example: shared
	$(CC) -Wall -O2 -g example/app.c -o example_app -Iinclude -L. -lpthread -lsupla -lssl

install: shared static
	@mkdir -p $(INSTALL_INCLUDE_PATH)
	cp -r include/libsupla $(INSTALL_INCLUDE_PATH)
	@mkdir -p $(INSTALL_LIBRARY_PATH)
	cp -a $(LIB_SHARED) $(LIB_SHARED_VERSION) $(LIB_SHARED_SO) $(INSTALL_LIBRARY_PATH)
	cp $(LIB_STATIC) $(INSTALL_LIBRARY_PATH)
	
uninstall:
	rm -rf $(INSTALL_INCLUDE_PATH)/libsupla
	rm -f $(INSTALL_LIBRARY_PATH)/libsupla.*

.PHONY: clean
clean:
	-${RM} $(LIB_STATIC) $(LIB_SHARED) $(LIB_SHARED_VERSION) $(LIB_SHARED_SO) $(OBJS) $(SRCS:.c=.d) 
	-${RM} $(SUPLACOREINCDIR) 
	-${RM} example_app
