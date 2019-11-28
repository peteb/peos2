#include <stdint.h>
#include <support/assert.h>
#include <support/pool.h>
#include <support/frag_buffer.h>
#include <support/flip_buffer.h>

#include "ipv4.h"
#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "tcp.h"
#include "udp.h"

#define NUM_BUFFERS 32

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
static void send_single_datagram(int interface,
                                 uint8_t protocol,
                                 uint32_t src_addr,
                                 uint32_t dest_addr,
                                 uint32_t ethernet_dest_ipaddr,
                                 const char *data,
                                 size_t length);

// Fragmentation reassembly state
static p2::pool<buffer, NUM_BUFFERS> used_buffers;  // Indexes point into `buffers`
static p2::frag_buffer<0xFFFF> buffers[NUM_BUFFERS];
static p2::flip_buffer<1024 * 10> arp_wait_buffer;  // Holding area for packets awaiting ARP

// State
static uint32_t local_ipaddr, local_netmask, local_gwaddr;
static uint16_t ip_id = 1;

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

  log(ipv4, "rx packet ver=%x,ihl=%x,ecn_dscp=%x,len=% 5d,id=%d,flags=%x,frag=%04x,prot=%d,ttl=%d,check=%04x,length=%d",
      hdr.version, hdr.ihl, hdr.ecn_dscp, hdr.total_len, hdr.id, flags, frag_offset, hdr.protocol, hdr.ttl, hdr.checksum, length);

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

  case PROTO_UDP:
    recv_function = udp_recv;
    break;

  default:
    log(ipv4, "unsupported protocol, dropping");
    return;
  }

  const char *payload = data + 4 * hdr.ihl;
  size_t payload_size = hdr.total_len - 4 * hdr.ihl;

  ipv4_dgram info;
  info.ttl = hdr.ttl;
  info.src_addr = hdr.src_addr;
  info.dest_addr = hdr.dest_addr;
  info.proto = hdr.protocol;

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

size_t ipv4_send(int interface, const ipv4_dgram &ipv4, const char *data, size_t length)
{
  uint32_t ethernet_dest_ipaddr = 0;

  if ((local_ipaddr & local_netmask) == (ipv4.dest_addr & local_netmask)) {
    // Same subnet
    ethernet_dest_ipaddr = ipv4.dest_addr;
  }
  else {
    // Different network
    ethernet_dest_ipaddr = local_gwaddr;
  }

  // Because the arp lookup is asynchronous
  auto data_handle = arp_wait_buffer.alloc(length);
  memcpy(arp_wait_buffer.data(data_handle), data, length);

  // TODO: skip copying to buffer if we don't have to do a request
  arp_request_lookup_ipv4(interface, ethernet_dest_ipaddr, [=](probe_result result) {
    if (result == PROBE_TIMEOUT) {
      log(ipv4, "ARP lookup timeout, dropping outgoing datagram");
      return;
    }

    char *payload = arp_wait_buffer.data(data_handle);

    if (!payload) {
      log(ipv4, "payload buffer disappeared while we waited for ARP, dropping outgoing datagram");
      return;
    }

    log(ipv4, "ARP lookup done, sending datagram");

    send_single_datagram(interface,
                         ipv4.proto,
                         ipv4.src_addr,
                         ipv4.dest_addr,
                         ethernet_dest_ipaddr,
                         payload,
                         length);
  });

  return 0;
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


static void send_single_datagram(int interface,
                                 uint8_t protocol,
                                 uint32_t src_addr,
                                 uint32_t dest_addr,
                                 uint32_t ethernet_dest_ipaddr,
                                 const char *data,
                                 size_t length)
{
  header hdr;
  hdr.ihl = 5; // 5 * 4 = 20 bytes
  hdr.version = 4;
  hdr.ecn_dscp = 0;
  hdr.total_len = htons(sizeof(hdr) + length);
  hdr.id = htons(ip_id++);
  hdr.frag_ofs = htons(0);
  hdr.ttl = 65;
  hdr.protocol = protocol;
  hdr.checksum = 0;  // Checksum needs to be 0 when calculating the checksum
  hdr.src_addr = htonl(src_addr);
  hdr.dest_addr = htonl(dest_addr);
  hdr.checksum = ipv4_checksum((char *)&hdr, sizeof(hdr), nullptr, 0);

  uint8_t dest[6], src[6];
  eth_hwaddr(interface, src);
  arp_cache_lookup_ipv4(interface, ethernet_dest_ipaddr, dest);

  eth_frame ethernet;
  ethernet.dest = dest;
  ethernet.src = src;
  ethernet.type = ET_IPV4;

  char buffer[1500];

  if (sizeof(hdr) + length > sizeof(buffer)) {
    log(ipv4, "datagram too large for one packet, dropping");
    return;
  }

  memcpy(buffer, &hdr, sizeof(hdr));
  memcpy(buffer + sizeof(hdr), data, length);

  eth_send(interface, &ethernet, buffer, sizeof(hdr) + length);
}

uint64_t sum_words(const char *data, size_t length)
{
  uint64_t sum = 0;

  // Warning: possibly non-aligned reads, but works on my CPU...
  const uint32_t *ptr32 = (const uint32_t *)data;

  while (length >= 4) {
    sum += *ptr32++;
    length -= 4;
  }

  // Might be leftovers
  const uint16_t *ptr16 = (const uint16_t *)ptr32;

  if (length >= 2) {
    sum += *ptr16;
    length -= 2;
  }

  // Might be even more leftovers (which should be padded)
  if (length > 0) {
    uint8_t byte = *(const uint8_t *)ptr16;
    sum += ntohs(byte << 8);
  }

  return sum;
}

uint16_t ipv4_checksum(const char *data, size_t length, const char *data2, size_t length2)
{
  uint32_t sum = 0;
  sum += sum_words(data, length);
  sum += sum_words(data2, length2);

  while (sum > 0xFFFF)
    sum = (sum >> 16) + (sum & 0xFFFF);

  return ~(uint16_t)sum;
}
