# -*- makefile -*-

LIB_INCLUDE_DIR=..
LIB_LIBRARY_DIR=..

SOURCES=utils.cc \
        tcp/receive_queue.cc tcp/send_queue.cc udp/protocol.cc \
        ethernet/protocol.cc ethernet/utils.cc arp/protocol.cc ipv4/utils.cc \
        ipv4/protocol.cc tcp/protocol.cc tcp/connection_state.cc tcp/connection_table.cc \
        tcp/connection.cc tcp/utils.cc ethernet/definitions.cc tcp/definitions.cc
SOURCES_unittest=$(SOURCES)

-include ../../Makefile.include

CXXFLAGS+=-I. -I../..
CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport

# Project specific rules
all :: $(OBJDIR)/libnet.a ;

$(OBJDIR)/libnet.a : CXXFLAGS+=-ffreestanding
$(OBJDIR)/libnet.a : $(OBJECTS)
	ar rcs $(OBJDIR)/libnet.a $(OBJECTS) $(LIB_LIBRARY_DIR)/support/$(OBJDIR)/libsupport.a
