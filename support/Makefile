# -*- makefile -*-

# Variables (open for modification)
LIBSUPPORT_SOURCES=utils.cc string.cc
TEST_SOURCES=$(wildcard unittests/test_*.cc)

CFLAGS=-std=gnu99 $(OPT_FLAGS) -Wall -Wextra -Werror -fno-threadsafe-statics -I. -I.. -DNDEBUG
CXXFLAGS=-std=c++17 $(OPT_FLAGS) -Wall -Wextra -Werror -fstrict-aliasing -Wstrict-aliasing=3 -fno-threadsafe-statics -fno-exceptions -fno-rtti -mno-red-zone -I.. -DNDEBUG

# Derivate vars (shouldn't contain static data)
OPT_FLAGS?=-O3
OBJDIR=.$(shell $(CC) -dumpmachine)
LIBSUPPORT_OBJECTS=$(addprefix $(OBJDIR)/,$(LIBSUPPORT_SOURCES:.cc=.o))
TEST_OBJECTS=$(addprefix $(OBJDIR)/,$(TEST_SOURCES:.cc=.o))

$(OBJDIR)/%.o : %.s
	@mkdir -p $(@D)
	$(AS) $< -o $@

$(OBJDIR)/%.o : %.c
	@mkdir -p $(@D)
	$(CC) -c $< -o $@ $(CFLAGS) -MMD -MP

$(OBJDIR)/%.o : %.cc
	@mkdir -p $(@D)
	$(CXX) -c $< -o $@ $(CXXFLAGS) -MMD -MP

.PHONY : clean

all : $(OBJDIR)/libsupport.a ;

unittest : $(OBJDIR)/unittest ;

check : $(OBJDIR)/unittest
	@$(OBJDIR)/unittest

clean :
	rm -rf $(OBJDIR)

$(OBJDIR)/libsupport.a : CXXFLAGS+=-ffreestanding
$(OBJDIR)/libsupport.a : $(LIBSUPPORT_OBJECTS)
	ar rcs $(OBJDIR)/libsupport.a $(LIBSUPPORT_OBJECTS)

$(OBJDIR)/libunittest.a : $(OBJDIR)/unittest.o
	ar rcs $(OBJDIR)/libunittest.a $(OBJDIR)/unittest.o

$(OBJDIR)/unittest : $(LIBSUPPORT_OBJECTS) $(TEST_OBJECTS) $(OBJDIR)/libunittest.a
	$(CXX) -o $(OBJDIR)/unittest -Werror $(OPT_FLAGS) $(LIBSUPPORT_OBJECTS) $(TEST_OBJECTS) -lunittest -L$(OBJDIR)

-include *.d
-include $(OBJDIR)/**/*.d
