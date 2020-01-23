# -*- makefile -*-

LIB_INCLUDE_DIR=../../libraries
LIB_LIBRARY_DIR=../../libraries

SOURCES_shell=shell.cc
SOURCES_shell_launcher=shell_launcher.cc

OBJECTS_shell=$(addprefix $(OBJDIR)/,$(SOURCES_shell:=.o))
OBJECTS_shell_launcher=$(addprefix $(OBJDIR)/,$(SOURCES_shell_launcher:=.o))

-include ../../Makefile.include

CRTI_OBJECT=$(OBJDIR)/crti.s.o
CRTN_OBJECT=$(OBJDIR)/crtn.s.o
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
OBJECTS_LINK_ORDER_shell=$(OBJECTS_shell) # TODO: include CRTI/CRTBEGIN/etc
OBJECTS_LINK_ORDER_shell_launcher=$(CRTI_OBJECT) $(CRTBEGIN_OBJECT) $(OBJECTS_shell_launcher) $(CRTEND_OBJECT) $(CRTN_OBJECT)

CXXFLAGS+=-I. -I../../
CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport

# Only build program for the target environment
ifneq ($HOSTED,1)
all : shell shell_launcher
endif

shell : CXXFLAGS+=-ffreestanding
shell : $(OBJECTS_shell) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER_shell) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -lgcc $(LINK_FLAGS)

shell_launcher : CXXFLAGS+=-ffreestanding
shell_launcher : $(OBJECTS_shell_launcher) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER_shell_launcher) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -lgcc $(LINK_FLAGS)
