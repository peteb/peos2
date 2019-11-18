# -*- makefile -*-

OPT_FLAGS ?= -O3
CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
CFLAGS=-std=gnu99 -masm=intel -ffreestanding $(OPT_FLAGS) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -masm=intel -ffreestanding $(OPT_FLAGS) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -I.. -DNDEBUG
OBJDIR=.target

# Code
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
INTERNAL_OBJECTS=boot.o main.o screen.o panic.o x86.o protected_mode.o multiboot.o \
		 keyboard.o syscalls.o filesystem.o terminal.o process.o memory.o \
		 ramfs.o init.o tar.o elf.o serial.o

OBJECTS=$(OBJDIR)/crti.o $(CRTBEGIN_OBJECT) $(addprefix $(OBJDIR)/,$(INTERNAL_OBJECTS)) $(CRTEND_OBJECT) $(OBJDIR)/crtn.o

all : vmpeoz

.PHONY : clean
clean :
	rm -rf $(OBJDIR) vmpeoz

$(OBJDIR)/%.o : %.s
	@mkdir -p $(@D)
	$(AS) $< -o $@

$(OBJDIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

$(OBJDIR)/%.o : %.cc
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP

vmpeoz : $(OBJECTS) linker.ld
	$(CC) -T linker.ld -o vmpeoz -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS) -L../support -lgcc -lsupport

-include $(OBJDIR)/*.d
-include $(OBJDIR)/*/*.d
