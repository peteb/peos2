# -*- makefile -*-

OPT_FLAGS ?= -O3
CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
CFLAGS=-std=gnu99 -m32 -masm=intel -nostdlib -ffreestanding $(OPT_FLAGS) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -m32 -masm=intel -nostdlib -ffreestanding $(OPT_FLAGS) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -I.. -DNDEBUG
LDFLAGS=-L../support  -lgcc
BINARIES=shell shell_launcher tester ls nsa
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
NSA_OBJECTS=net/main.o net/ethernet.o net/utils.o net/arp.o net/ipv4.o net/tcp.o net/udp.o

.PHONY : clean

%.o : %.s
	$(AS) $< -o $@

%.o : %.c
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

%.o : %.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP

all : $(BINARIES)

clean :
	rm -rf *.o *.d $(BINARIES)

shell : shell.o
	$(CC) -o $@ $(CXXFLAGS) shell.o $(LDFLAGS) -lsupport

shell_launcher : shell_launcher.o
	$(CC) -o $@ $(CXXFLAGS) shell_launcher.o $(LDFLAGS) -lsupport

tester : tester.o
	$(CC) -o $@ $(CXXFLAGS) tester.o $(LDFLAGS) -lsupport

nsa : $(NSA_OBJECTS) crti.o crtn.o linker.ld
	$(CC) -T linker.ld -o $@ $(CXXFLAGS) crti.o $(CRTBEGIN_OBJECT) $(NSA_OBJECTS) -lgcc $(CRTEND_OBJECT) crtn.o -L../support -lsupport

ls : ls.o
	$(CC) -o $@ $(CXXFLAGS) ls.o $(LDFLAGS) -lsupport

-include *.d
-include */*.d
