# -*- makefile -*-

# Commands
CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
CFLAGS=-std=gnu99 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -I.. -DNDEBUG
OBJDIR=.target

# Code
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
INTERNAL_OBJECTS=boot.o main.o screen.o panic.o x86.o protected_mode.o multiboot.o \
		 keyboard.o syscalls.o filesystem.o terminal.o process.o memory.o \
		 ramfs.o init.o tar.o elf.o

OBJECTS=$(OBJDIR)/crti.o $(CRTBEGIN_OBJECT) $(addprefix $(OBJDIR)/,$(INTERNAL_OBJECTS)) $(CRTEND_OBJECT) $(OBJDIR)/crtn.o
OPT_LEVEL=-O3

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

ifneq ($(OPTLEVEL),)
OPT_LEVEL=$(OPTLEVEL)
endif

vmpeoz : $(OBJECTS) linker.ld
	$(CC) -T linker.ld -o vmpeoz -ffreestanding $(OPT_LEVEL) -Werror -nostdlib $(OBJECTS) -L../support -lgcc -lsupport
