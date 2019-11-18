# -*- makefile -*-

CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
CFLAGS=-std=gnu99 -m32 -masm=intel -nostdlib -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -m32 -masm=intel -nostdlib -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -I.. -DNDEBUG
LDFLAGS=-L../support  -lgcc

.PHONY : clean

%.o : %.s
	$(AS) $< -o $@

%.o : %.c
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

%.o : %.cc
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP

all : shell tester

clean :
	rm -rf *.o *.d shell tester

shell : shell.o
	$(CC) -o $@ $(CXXFLAGS) shell.o $(LDFLAGS) -lsupport

tester : tester.o
	$(CC) -o $@ $(CXXFLAGS) tester.o $(LDFLAGS) -lsupport
