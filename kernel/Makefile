# -*- makefile -*-

LIB_INCLUDE_DIR=../libraries
LIB_LIBRARY_DIR=../libraries

SOURCES=boot.s main.cc screen.cc panic.cc x86.cc protected_mode.cc multiboot.cc \
		    keyboard.cc syscalls.cc filesystem.cc terminal.cc process.cc memory.cc \
		    ramfs.cc init.cc tar.cc elf.cc serial.cc pci.cc rtl8139.cc locks.cc timer.cc \

-include ../Makefile.include

CRTI_OBJECT=$(OBJDIR)/crti.s.o
CRTN_OBJECT=$(OBJDIR)/crtn.s.o
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
OBJECTS_LINK_ORDER=$(CRTI_OBJECT) $(CRTBEGIN_OBJECT) $(OBJECTS) $(CRTEND_OBJECT) $(CRTN_OBJECT)

# TODO: revert all inline asm into AT&T syntax
CXXFLAGS+=-masm=intel -I.

# Only build the kernel for the non-hosted environment
ifneq ($HOSTED,1)
all : vmpeoz
endif

vmpeoz : CXXFLAGS+=-ffreestanding
vmpeoz : $(OBJECTS) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o vmpeoz -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -lgcc -lsupport

