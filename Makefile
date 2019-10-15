CC=i686-elf-gcc
CXX=i686-elf-g++
AS=i686-elf-as
CFLAGS=-std=gnu99 -O2 -Wall -Wextra
CXXFLAGS=-std=c++17 -O2 -Wall -Wextra -fno-exceptions -fno-rtti

all : vmpeoz

OBJECTS=boot.o main.o

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
	$(CC) -T linker.ld -o vmpeoz -ffreestanding -O2 -nostdlib $(OBJECTS) -lgcc
