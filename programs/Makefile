# Commands
CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
OPT_LEVEL=-g
CFLAGS=-std=gnu99 -m32 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-threadsafe-statics -DNDEBUG
CXXFLAGS=-std=c++17 -m32 -masm=intel -ffreestanding $(OPT_LEVEL) -Wall -Wextra -Werror -fno-exceptions -fno-rtti -fno-threadsafe-statics -mno-red-zone -mgeneral-regs-only -I. -DNDEBUG

OBJECTS=first_program.o

all : first_program

.PHONY : clean
clean :
	rm first_program *.o *.d

%.o : %.cc
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP

first_program : $(OBJECTS)
	$(CC) -o first_program -m32 -ffreestanding -Werror -nostdlib $(OBJECTS) -lgcc
