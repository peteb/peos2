# -*- makefile -*-

LIB_INCLUDE_DIR=../../libraries
LIB_LIBRARY_DIR=../../libraries

SOURCES=main.cc

-include ../../Makefile.include

CRTI_OBJECT=$(OBJDIR)/crti.s.o
CRTN_OBJECT=$(OBJDIR)/crtn.s.o
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
OBJECTS_LINK_ORDER=$(CRTI_OBJECT) $(CRTBEGIN_OBJECT) $(OBJECTS) $(CRTEND_OBJECT) $(CRTN_OBJECT)

CXXFLAGS+=-I. -I../../
CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport -lnet

# Only build program for the target environment
ifneq ($HOSTED,1)
all : nsa
endif

nsa : CXXFLAGS+=-ffreestanding
nsa : $(OBJECTS) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -L$(LIB_LIBRARY_DIR)/net/$(OBJDIR) -lgcc $(LINK_FLAGS)

