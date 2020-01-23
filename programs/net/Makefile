# -*- makefile -*-

LIB_INCLUDE_DIR=../../libraries
LIB_LIBRARY_DIR=../../libraries

SOURCES=main.cc ethernet.cc utils.cc arp.cc ipv4.cc tcp.cc udp.cc \
        tcp_connection.cc tcp_connection_table.cc tcp_recv_queue.cc \
        tcp_connection_state.cc tcp_send_queue.cc tcp_proto.cc ipv4_protocol.cc

SOURCES_unittest=tcp_connection_table.cc tcp_connection.cc tcp_recv_queue.cc \
                 tcp_send_queue.cc tcp_proto.cc utils.cc ipv4_protocol.cc

-include ../../Makefile.include

CRTI_OBJECT=$(OBJDIR)/crti.s.o
CRTN_OBJECT=$(OBJDIR)/crtn.s.o
CRTBEGIN_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtbegin.o)
CRTEND_OBJECT=$(shell $(CC) $(CFLAGS) -print-file-name=crtend.o)
OBJECTS_LINK_ORDER=$(CRTI_OBJECT) $(CRTBEGIN_OBJECT) $(OBJECTS) $(CRTEND_OBJECT) $(CRTN_OBJECT)

CXXFLAGS+=-I. -I../../
CXXFLAGS+=-masm=intel
LINK_FLAGS+=-lsupport

# Only build program for the target environment
ifneq ($HOSTED,1)
all : nsa
endif

nsa : CXXFLAGS+=-ffreestanding
nsa : $(OBJECTS) $(CRTI_OBJECT) $(CRTN_OBJECT) linker.ld
	$(CC) -T linker.ld -o $@ -ffreestanding $(OPT_FLAGS) -Werror -nostdlib $(OBJECTS_LINK_ORDER) -L$(LIB_LIBRARY_DIR)/support/$(OBJDIR) -lgcc $(LINK_FLAGS)

