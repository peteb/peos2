CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
OPT_LEVEL=-O3
CFLAGS=-std=gnu99 -masm=intel $(OPT_LEVEL) -Wall -Wextra -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -masm=intel $(OPT_LEVEL) -Wall -Wextra -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -DNDEBUG

all : vmpeoz

CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
INTERNAL_OBJECTS=boot.o main.o screen.o panic.o x86.o protected_mode.o multiboot.o \
		 keyboard.o syscalls.o filesystem.o support/utils.o support/string.o

OBJECTS=crti.o $(CRTBEGIN_OBJECT) $(INTERNAL_OBJECTS) $(CRTEND_OBJECT) crtn.o

.PHONY : clean
clean :
	rm -f *.o vmpeoz

%.o : %.s
	$(AS) $< -o $@

%.o : %.c
	$(CC) -c $< -o $@ -ffreestanding $(CFLAGS)

%.o : %.cc
	$(CXX) -c $< -o $@ -ffreestanding $(CXXFLAGS)

vmpeoz : $(OBJECTS) linker.ld
	$(CC) -T linker.ld -o vmpeoz -ffreestanding $(OPT_LEVEL) -nostdlib $(OBJECTS) -lgcc
