#include <stdint.h>

#include "ipv4.h"
#include "arp.h"
#include "utils.h"
#include "ethernet.h"

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
  (void)data;
  (void)length;

  log(ipv4, "rx packet");
}

int ipv4_local_address(uint32_t *addr)
{
  *addr = local_ipaddr;
  return 0;
}
