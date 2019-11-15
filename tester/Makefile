# -*- makefile -*-

CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
OPT_LEVEL=-g -O0
CFLAGS=-std=gnu99 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -I.. -DNDEBUG
OBJDIR=.target

OBJECTS=main.o

.PHONY : clean

$(OBJDIR)/%.o : %.s
	@mkdir -p $(@D)
	$(AS) $< -o $@

$(OBJDIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

$(OBJDIR)/%.o : %.cc
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP

OBJECT_PATHS=$(addprefix $(OBJDIR)/,$(OBJECTS))

all : tester

clean :
	rm -rf $(OBJDIR) tester

tester : $(OBJECT_PATHS)
	$(CC) -o tester -m32 -ffreestanding -Werror -nostdlib $(OBJECT_PATHS) -L../support -lgcc -lsupport
