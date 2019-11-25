#include <stdint.h>
#include <support/assert.h>
#include <support/pool.h>

#include "ipv4.h"
#include "arp.h"
#include "utils.h"
#include "ethernet.h"
#include "tcp.h"

#define NUM_BUFFERS 32

#define PROTO_ICMP   1
#define PROTO_TCP    6
#define PROTO_UDP   17

struct header {
  uint8_t  ihl:4;
  uint8_t  version:4;

  uint8_t  ecn:2;
  uint8_t  dscp:6;

  uint16_t total_len;
  uint16_t id;

  uint8_t  flags:3;
  uint16_t frag_ofs:13;

  uint8_t  ttl;
  uint8_t  protocol;

  uint16_t checksum;

  uint32_t src_addr;
  uint32_t dest_addr;
};

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

  log(ipv4, "rx packet ver=%x,ihl=%x,dscp=%x,ecn=%x,len=%x,id=%x,flags=%x,frag=%x,prot=%d,ttl=%d,check=%x",
      hdr.version, hdr.ihl, hdr.dscp, hdr.ecn, hdr.total_len, hdr.id, hdr.flags, hdr.frag_ofs, hdr.protocol, hdr.ttl, hdr.checksum);

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

  const char *payload = data + 4 * hdr.ihl;
  size_t payload_size = length - 4 * hdr.ihl;
  size_t total_data_len = hdr.total_len - hdr.ihl * 4;

  if (hdr.protocol == PROTO_TCP) {
    if (total_data_len == payload_size) {
      // All data was contained in this packet, so we can just forward it
      ipv4_dgram info;
      info.ttl = hdr.ttl;
      info.src_addr = hdr.src_addr;
      info.dest_addr = hdr.dest_addr;

      tcp_recv(interface, frame, &info, payload, payload_size);
    }
    else {
      // Got to reassemble the datagram from fragments
    }
  }
  else {
    log(ipv4, "unknown protocol, dropping");
    return;
  }
}

void ipv4_tick()
{
}

int ipv4_local_address(uint32_t *addr)
{
  *addr = local_ipaddr;
  return 0;
}
