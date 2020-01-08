# -*- makefile -*-

SOURCES=utils.cc string.cc

# <hack_to_build_libunittest>
OBJDIR=.$(shell $(CC) -dumpmachine)
unittest :: $(OBJDIR)/libunittest.a ;
# </hack_to_build_libunittest>

-include ../../Makefile.include

# Project specific rules
all :: $(OBJDIR)/libsupport.a ;

# There's no freestanding version of libunittest, so only include if we're building for the host environment
ifeq ($HOSTED,1)
all :: $(OBJDIR)/libunittest.a ;
endif

# libsupport.a is only built with freestanding as it's only used by the kernel
$(OBJDIR)/libsupport.a : CXXFLAGS+=-ffreestanding
$(OBJDIR)/libsupport.a : $(OBJECTS)
	ar rcs $(OBJDIR)/libsupport.a $(OBJECTS)

# The unittest framework used by this and other projects
$(OBJDIR)/libunittest.a : $(OBJDIR)/unittest.o
	ar rcs $(OBJDIR)/libunittest.a $(OBJDIR)/unittest.o
