all: _all

-include build/Custom.mk

NAME?= teexec
OS:= $(shell uname -s)
ARCH?= $(shell uname -m)
ifeq ($(ARCH_TARGET),x86_32)
ARCHFLAGS?= -m32
else
ARCHFLAGS?= -m64
endif

BIN:= build/bin/$(NAME)
CFG:= build/tmp/config.h
MAP:= build/$(OS).map

CFLAGS:= -std=gnu11 -MMD -fPIC -fvisibility=hidden $(ARCHFLAGS)
ifeq ($(BUILD),debug)
  CFLAGS+= -Wall -Wextra -Werror -g
else
  OPTFLAGS:= -O2
  ifeq ($(findstring BSD,$(OS)),)
    OPTFLAGS+= -flto
  endif
  CFLAGS+= $(OPTFLAGS)
endif

LDFLAGS:= $(OPTFLAGS)
ifneq ($(wildcard $(MAP)),)
  LDFLAGS+= -Wl,--version-script,$(MAP)
endif

ifeq ($(OS),Linux)
  CFLAGS+= -D_GNU_SOURCE
  LDFLAGS+= -ldl
endif

ifeq ($(OS),Darwin)
  LIBNAME:= $(NAME).dylib
  SOFLAGS:= -dynamiclib -current_version 1.0 -compatibility_version 1.0
else
  SOFLAGS:= -shared -nostdlib
endif

BINSRC:= main.c cmd.c proc.c sock.c debug.c
LIBSRC:= trace.c hoist.c debug.c sock.c
ifeq ($(LIBNAME),)
  BINFLAGS:= -pie -Wl,-E $(LDFLAGS)
  BINSRC:= $(sort $(LIBSRC) $(BINSRC))
  LIBSRC:=
else
  CFLAGS+= -DLIBNAME='"$(LIBNAME)"'
  BINFLAGS:= $(CFLAGS)
  LIBFLAGS:= $(LDFLAGS) $(SOFLAGS)
  LIB:= build/lib/$(LIBNAME)
endif
BINOBJ:= $(BINSRC:%.c=build/tmp/%.o)
LIBOBJ:= $(LIBSRC:%.c=build/tmp/%.o)
DEP:= $(BINOBJ:%.o=%.d) $(LIBOBJ:%.o=%.d)

_all: $(BIN) $(LIB)

$(BIN): $(BINOBJ) | build/bin
	$(CC) $^ -o $@ $(BINFLAGS)

$(LIB): $(LIBOBJ) | build/lib
	$(CC) $^ -o $@ $(LIBFLAGS)

$(CFG): build/config.py | build/tmp
	python $< > $@

build/tmp/%.o: src/%.c $(CFG) | build/tmp
	$(CC) -c $<	-o $@	$(CFLAGS) -include $(CFG)

build/bin build/lib build/tmp:
	mkdir $@

clean:
	rm -rf build/tmp build/bin build/lib

.PHONY: all _all run clean

-include $(DEP)
