# -*- makefile -*-

LIB_INCLUDE_DIR=..
LIB_LIBRARY_DIR=..

SOURCES=ethernet.cc utils.cc arp.cc ipv4.cc tcp.cc udp.cc \
        tcp_connection.cc tcp_connection_table.cc tcp_recv_queue.cc \
        tcp_connection_state.cc tcp_send_queue.cc tcp_proto.cc ipv4_protocol.cc \
        ethernet/protocol.cc ethernet/utils.cc arp/protocol.cc ipv4/utils.cc
SOURCES_unittest=tcp_connection_table.cc tcp_connection.cc tcp_recv_queue.cc \
                 tcp_send_queue.cc tcp_proto.cc utils.cc ipv4_protocol.cc

-include ../../Makefile.include

CXXFLAGS+=-I. -I../..
CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport

# Project specific rules
all :: $(OBJDIR)/libnet.a ;

$(OBJDIR)/libnet.a : CXXFLAGS+=-ffreestanding
$(OBJDIR)/libnet.a : $(OBJECTS)
	ar rcs $(OBJDIR)/libnet.a $(OBJECTS) $(LIB_LIBRARY_DIR)/support/$(OBJDIR)/libsupport.a
