# -*- makefile -*-

LIB_INCLUDE_DIR=../../libraries
LIB_LIBRARY_DIR=../../libraries

SOURCES=main.cc

-include ../../Makefile.include

CRTI_OBJECT=$(OBJDIR)/crti.s.o
CRTN_OBJECT=$(OBJDIR)/crtn.s.o
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)

CXXFLAGS+=-I. -I../../
CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport -lnet

# TODO: use sysroot instead so we don't have to mess around with this order
OBJECTS_LINK_ORDER=$(CRTI_OBJECT) $(CRTBEGIN_OBJECT) \
                   $(OBJECTS) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -L$(LIB_LIBRARY_DIR)/net/$(OBJDIR) -lgcc $(LINK_FLAGS) \
                   $(CRTEND_OBJECT) $(CRTN_OBJECT)

# Only build program for the target environment
ifneq ($HOSTED,1)
all : live-httpd
endif

live-httpd : CXXFLAGS+=-ffreestanding
live-httpd : $(OBJECTS) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER)

