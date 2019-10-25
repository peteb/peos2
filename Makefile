# -*- makefile -*-

# Commands
CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
CFLAGS=-std=gnu99 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -DNDEBUG
OBJDIR=target

# Code
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
INTERNAL_OBJECTS=boot.o main.o screen.o panic.o x86.o protected_mode.o multiboot.o \
		 keyboard.o syscalls.o filesystem.o
OBJECTS=$(OBJDIR)/crti.o $(CRTBEGIN_OBJECT) $(addprefix $(OBJDIR)/,$(INTERNAL_OBJECTS)) $(UNITTESTABLE_OBJECTS) $(CRTEND_OBJECT) $(OBJDIR)/crtn.o
BINARIES=vmpeoz

all : vmpeoz

include Makefile.common

vmpeoz : $(OBJECTS) linker.ld
	$(CC) -T linker.ld -o vmpeoz -ffreestanding $(OPT_LEVEL) -Werror -nostdlib $(OBJECTS) -lgcc
