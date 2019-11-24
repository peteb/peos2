#include <support/utils.h>
#include <support/assert.h>
#include <support/queue.h>

#include "arp.h"
#include "ethernet.h"
#include "utils.h"

struct header {
  uint16_t htype;
  uint16_t ptype;
  uint8_t  hlen, plen;
  uint16_t oper;
  uint8_t  sha[6];
  uint32_t spa;
  uint8_t  tha[6];
  uint32_t tpa;
} __attribute__((packed));

struct cache_entry {
  uint32_t ipaddr;
  uint8_t  hwaddr[6];
};

// Declarations
static void add_cache_entry(uint32_t ipaddr, const uint8_t *hwaddr);
static const cache_entry *fetch_cache_entry(uint32_t ipaddr);
static void send_ipv4_request(int fd, uint32_t ipaddr);


// Global state
static p2::queue<cache_entry, 64> ipv4_entries;
// TODO: replace this cache with something that doesn't require brute force search

// Definitions
void arp_recv(int eth_fd, eth_frame */*frame*/, const char *data, size_t length)
{
  header hdr;
  assert(length >= sizeof(hdr));
  memcpy(&hdr, data, sizeof(hdr));

  uint16_t htype = ntohs(hdr.htype);
  uint16_t ptype = ntohs(hdr.ptype);
  uint16_t oper = ntohs(hdr.oper);

  if (htype != 1 || ptype != ET_IPV4) {
    log(arp, "protocols not supported htype=%04x,ptype=%04x",
        htype, ptype);
    return;
  }

  if (hdr.hlen != 6 || hdr.plen != 4) {
    log(arp, "address sizes not supported hlen=%02x,ptype=%02x",
        hdr.hlen, hdr.plen);
    return;
  }

  uint8_t local_hwaddr[6];
  eth_hwaddr(eth_fd, local_hwaddr);

  if (memcmp(local_hwaddr, hdr.sha, 6) == 0) {
    log(arp, "message from ourselves, ignoring");
    return;
  }

  uint32_t spa = ntohl(hdr.spa);
  uint32_t tpa = ntohl(hdr.tpa);

  if (oper == 1) {
    // Request
    log(arp, "ipv4->eth sha=%s spa=%s tha=%s tpa=%s",
        hwaddr_str(hdr.sha).c_str(),
        ipaddr_str(spa).c_str(),
        hwaddr_str(hdr.tha).c_str(),
        ipaddr_str(tpa).c_str());

    // TODO: ask the IP stack if this IP address is us. If it is, then reply
  }
  else if (oper == 2) {
    // Reply
    log(arp, "eth->ipv4 sha=%s spa=%s tha=%s tpa=%s",
        hwaddr_str(hdr.sha).c_str(),
        ipaddr_str(spa).c_str(),
        hwaddr_str(hdr.tha).c_str(),
        ipaddr_str(tpa).c_str());

    // TODO: populate our cache with this mapping
    add_cache_entry(spa, hdr.sha);
  }
  else {
    log(arp, "invalid oper=%04x", oper);
    return;
  }
}

void arp_tick()
{
  log(arp, "checking for timed out actions");
}

int arp_lookup_ipv4(int fd, uint32_t ipaddr, uint8_t *hwaddr)
{
  // TODO: cache per fd
  if (const cache_entry *entry = fetch_cache_entry(ipaddr)) {
    memcpy(hwaddr, entry->hwaddr, 6);
    return 1;
  }

  // TODO: check if we already have an outstanding request for this
  // address and restart it
  send_ipv4_request(fd, ipaddr);
  return 0;
}

static void send_ipv4_request(int fd, uint32_t ipaddr)
{
  header hdr;
  hdr.htype = htons(1);
  hdr.ptype = htons(ET_IPV4);
  hdr.hlen = 6;
  hdr.plen = 4;
  hdr.oper = htons(1);
  eth_hwaddr(fd, hdr.sha);
  hdr.spa = 0;  // TODO: fill in IP address?
  memset(hdr.tha, 0, sizeof(hdr.tha));
  hdr.tpa = htonl(ipaddr);

  uint8_t bdxaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

  eth_frame frame = {
    .dest = bdxaddr,
    .src = hdr.sha,
    .type = ET_ARP
  };

  int ret = eth_send(fd, &frame, (char *)&hdr, sizeof(hdr));
  assert(ret >= 0);
}

static void add_cache_entry(uint32_t ipaddr, const uint8_t *hwaddr)
{
  cache_entry entry;
  entry.ipaddr = ipaddr;
  memcpy(entry.hwaddr, hwaddr, sizeof(entry.hwaddr));

  if (ipv4_entries.full())
    ipv4_entries.pop_front();

  log(arp, "writing cache %s=%s", ipaddr_str(ipaddr).c_str(), hwaddr_str(hwaddr).c_str());
  ipv4_entries.push_back(entry);
}

static const cache_entry *fetch_cache_entry(uint32_t ipaddr)
{
  const cache_entry *latest_entry = nullptr;

  // We need to always iterate through the *whole* cache as a key
  // might've received a new value...
  for (int i = ipv4_entries.begin(); i != ipv4_entries.end(); ++i) {
    if (ipv4_entries[i].ipaddr == ipaddr)
      latest_entry = &ipv4_entries[i];
  }

  return latest_entry;
}
