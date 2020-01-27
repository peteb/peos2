# -*- makefile -*-

SOURCES=ls.cc

-include ../Makefile.include

CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport

# Only build program for the target environment
ifneq ($HOSTED,1)
all : ls
endif

ls : CXXFLAGS+=-ffreestanding
ls : $(OBJECTS) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -lgcc $(LINK_FLAGS)

