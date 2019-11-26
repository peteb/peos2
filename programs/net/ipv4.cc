#include <stdint.h>
#include <support/assert.h>
#include <support/pool.h>
#include <support/frag_buffer.h>

#include "ipv4.h"
#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "tcp.h"

#define NUM_BUFFERS 32

#define PROTO_ICMP   1
#define PROTO_TCP    6
#define PROTO_UDP   17

#define FLAGS_DF  0x01
#define FLAGS_MF  0x02

struct header {
  uint8_t  ihl:4;
  uint8_t  version:4;
  uint8_t  ecn_dscp;
  uint16_t total_len;
  uint16_t id;
  uint16_t frag_ofs;
  uint8_t  ttl;
  uint8_t  protocol;
  uint16_t checksum;
  uint32_t src_addr;
  uint32_t dest_addr;
} __attribute__((packed));

struct buffer_id {
  uint32_t src, dest;
  uint16_t id;

  bool operator ==(const buffer_id &other) const
  {
    return src == other.src && dest == other.dest && id == other.id;
  }
};

struct buffer {
  buffer_id key;
  int ttl;
};

// Declarations
static uint16_t fetch_or_create_buffer(const buffer_id &bufid);

// Fragmentation reassembly state
static p2::pool<buffer, NUM_BUFFERS> used_buffers;  // Indexes point into `buffers`
static p2::frag_buffer<0xFFFF> buffers[NUM_BUFFERS];

// State
static uint32_t local_ipaddr, local_netmask, local_gwaddr;

void ipv4_configure(int interface, uint32_t ipaddr, uint32_t netmask, uint32_t gwaddr)
{
  local_ipaddr = ipaddr;
  local_netmask = netmask;
  local_gwaddr = gwaddr;

  log(ipv4, "config ipaddr=%s,mask=%s,gw=%s", ipaddr_str(ipaddr), ipaddr_str(netmask), ipaddr_str(gwaddr));

  // Run an ARP request for our own ip address to see that it's not
  // already in use. We're sending from our configured IP address, so
  // it's also a gratuitious ARP. TODO: be a bit nicer
  arp_request_lookup_ipv4(interface, ipaddr, [=](probe_result result) {
    if (result != PROBE_TIMEOUT) {
      uint8_t remote_hwaddr[6];
      arp_cache_lookup_ipv4(interface, ipaddr, remote_hwaddr);
      log(ipv4, "IPv4 address collision with %s", hwaddr_str(remote_hwaddr));
    }
  });

  // Check that the default gateway is reachable. Also good to get the
  // gateway's MAC address in our cache
  arp_request_lookup_ipv4(interface, gwaddr, [=](probe_result result) {
    if (result == PROBE_TIMEOUT) {
      log(ipv4, "failed to lookup default gateway hwaddr");
    }
    else {
      log(ipv4, "default gateway located");
    }
  });
}

void ipv4_recv(int interface, eth_frame *frame, const char *data, size_t length)
{
  (void)interface;
  (void)frame;
  assert(length > sizeof(header));

  header hdr;
  memcpy(&hdr, data, sizeof(header));
  hdr.total_len = ntohs(hdr.total_len);
  hdr.id = ntohs(hdr.id);
  hdr.frag_ofs = ntohs(hdr.frag_ofs);
  hdr.checksum = ntohs(hdr.checksum);
  hdr.src_addr = ntohl(hdr.src_addr);
  hdr.dest_addr = ntohl(hdr.dest_addr);

  uint8_t flags = hdr.frag_ofs >> 12;
  uint16_t frag_offset = (hdr.frag_ofs & 0x1FFF) * 8;

  log(ipv4, "rx packet ver=%x,ihl=%x,ecn_dscp=%x,len=% 5d,id=%d,flags=%x,frag=%04x,prot=%d,ttl=%d,check=%04x",
      hdr.version, hdr.ihl, hdr.ecn_dscp, hdr.total_len, hdr.id, flags, frag_offset, hdr.protocol, hdr.ttl, hdr.checksum);

  if (hdr.version != 4) {
    log(ipv4, "incorrect version, dropping");
    return;
  }

  if (hdr.ihl > 8) {
    log(ipv4, "header too large, dropping");
    return;
  }

  if (hdr.total_len < hdr.ihl * 4) {
    log(ipv4, "total datagram length is less than the header length, dropping");
    return;
  }

  void (*recv_function)(int, eth_frame *, ipv4_dgram *, const char *, size_t);

  switch (hdr.protocol) {
  case PROTO_TCP:
    recv_function = tcp_recv;
    break;

  default:
    log(ipv4, "unsupported protocol, dropping");
    return;
  }

  const char *payload = data + 4 * hdr.ihl;
  size_t payload_size = length - 4 * hdr.ihl;

  ipv4_dgram info;
  info.ttl = hdr.ttl;
  info.src_addr = hdr.src_addr;
  info.dest_addr = hdr.dest_addr;

  if (!(flags & FLAGS_MF) && hdr.frag_ofs == 0) {
    // All data was contained in this packet, so we can just forward it
    recv_function(interface, frame, &info, payload, payload_size);
  }
  else {
    // Got to reassemble the datagram from fragments
    uint16_t buf_idx = fetch_or_create_buffer({hdr.src_addr, hdr.dest_addr, hdr.id});
    assert(buf_idx != used_buffers.end());
    buffers[buf_idx].insert(frag_offset, payload, payload_size);

    if (!(flags & FLAGS_MF)) {
      // Last fragment
      log(ipv4, "reassembled datagram into %d bytes", buffers[buf_idx].continuous_size());
      recv_function(interface,
                    frame,
                    &info,
                    (char *)buffers[buf_idx].data(),
                    buffers[buf_idx].continuous_size());
      used_buffers.erase(buf_idx);
    }
  }
}

void ipv4_tick(int dt)
{
  for (size_t i = 0; i < used_buffers.watermark(); ++i) {
    if (!used_buffers.valid(i))
      continue;

    if ((used_buffers[i].ttl -= dt) <= 0) {
      log(ipv4, "reassembly of %s %s [%d] aborted due to timeout",
          ipaddr_str(used_buffers[i].key.src),
          ipaddr_str(used_buffers[i].key.dest),
          used_buffers[i].key.id);

      used_buffers.erase(i);
    }
  }
}

int ipv4_local_address(uint32_t *addr)
{
  *addr = local_ipaddr;
  return 0;
}

static uint16_t fetch_or_create_buffer(const buffer_id &bufid)
{
  // TODO: O(1) lookup...
  for (size_t i = 0; i < used_buffers.watermark(); ++i) {
    if (!used_buffers.valid(i))
      continue;

    if (used_buffers[i].key == bufid)
      return i;
  }

  uint16_t idx = used_buffers.push_back({bufid, 250});
  buffers[idx].reset();
  return idx;
}
