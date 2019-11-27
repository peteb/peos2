// -*- c++ -*-

#ifndef NET_TCP_PROTO_H
#define NET_TCP_PROTO_H

#include "net/ipv4.h"
#include "net/ethernet.h"

#define FIN 0x0001
#define SYN 0x0002
#define RST 0x0004
#define PSH 0x0008
#define ACK 0x0010
#define URG 0x0020
#define ECE 0x0040
#define CWR 0x0080
#define NS  0x0100

struct tcp_header {
  uint16_t src_port;
  uint16_t dest_port;
  uint32_t seq_nbr;
  uint32_t ack_nbr;
  uint16_t flags_hdrsz;
  uint16_t wndsz;
  uint16_t checksum;
  uint16_t urgptr;
} __attribute__((packed));

struct tcp_endpoint {
  uint32_t ipaddr;
  uint16_t port;
};

struct tcp_segment {
  eth_frame *frame;
  ipv4_dgram *datagram;
  tcp_header *tcphdr;
  const char *payload;
  size_t payload_size;
};

typedef uint32_t tcp_seqnbr;


#endif // !NET_TCP_PROTO_H
