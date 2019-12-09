#include <support/utils.h>
#include <support/assert.h>
#include <support/queue.h>
#include <support/pool.h>

#include "arp.h"
#include "ethernet.h"
#include "utils.h"
#include "retryable.h"
#include "ipv4.h"

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
typedef retryable<probe_result> probe;

static void               add_cache_entry(uint32_t ipaddr, const uint8_t *hwaddr);
static const cache_entry *fetch_cache_entry(uint32_t ipaddr);
static void               send_ipv4_request(int fd, uint32_t ipaddr);
static void               send_ipv4_reply(int fd, uint32_t ipaddr, const uint8_t *hwaddr);
static size_t             find_ipv4_probe(uint32_t ipaddr);

// Global state
static p2::queue<cache_entry, 64> ipv4_entries;
// TODO: replace this cache with something that doesn't require brute force search


struct ipaddr_probe {
  ipaddr_probe(uint32_t ipaddr, probe probe_)
    : key(ipaddr), value(probe_) {}

  uint32_t key;
  probe value;
};


static p2::pool<ipaddr_probe, 64> ipv4_probes;  // TODO: replace with iterable hash?

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

    uint32_t local_addr;

    if (ipv4_local_address(&local_addr) >= 0) {
      send_ipv4_reply(eth_fd, spa, hdr.tha);
    }
  }
  else if (oper == 2) {
    // Reply
    log(arp, "eth->ipv4 sha=%s spa=%s tha=%s tpa=%s",
        hwaddr_str(hdr.sha).c_str(),
        ipaddr_str(spa).c_str(),
        hwaddr_str(hdr.tha).c_str(),
        ipaddr_str(tpa).c_str());

    add_cache_entry(spa, hdr.sha);

    if (size_t probe_idx = find_ipv4_probe(spa); probe_idx != ipv4_probes.end_sentinel()) {
      ipv4_probes[probe_idx].value.notify_all(PROBE_REPLY);
      ipv4_probes.erase(probe_idx);
    }
  }
  else {
    log(arp, "invalid oper=%04x", oper);
    return;
  }
}

void arp_tick(int ticks)
{
  for (size_t i = 0; i < ipv4_probes.watermark(); ++i) {
    if (!ipv4_probes.valid(i))
      continue;

    if (!ipv4_probes[i].value.tick(ticks)) {
      ipv4_probes[i].value.notify_all(PROBE_TIMEOUT);
      ipv4_probes.erase(i);
    }
  }
}

int arp_cache_lookup_ipv4(int fd, uint32_t ipaddr, uint8_t *hwaddr)
{
  (void)fd;

  if (const cache_entry *entry = fetch_cache_entry(ipaddr)) {
    memcpy(hwaddr, entry->hwaddr, 6);
    return 1;
  }

  return 0;
}

int arp_request_lookup_ipv4(int fd, uint32_t ipaddr, probe::await_fun callback)
{
  // TODO: cache per fd
  if (fetch_cache_entry(ipaddr)) {
    callback(PROBE_REPLY);
    return 1;
  }

  if (size_t probe_idx = find_ipv4_probe(ipaddr); probe_idx != ipv4_probes.end_sentinel()) {
    ipv4_probes[probe_idx].value.reset();
    ipv4_probes[probe_idx].value.await(callback);
  }
  else {
    // TODO: get rid of all copies
    probe::op_fun op = [=]() {send_ipv4_request(fd, ipaddr); };
    int idx = ipv4_probes.emplace_anywhere(ipaddr, op);
    ipv4_probes[idx].value.await(callback);
    ipv4_probes[idx].value.tick(0);
  }

  return 0;
}

static int send_arp(int fd, int op, uint32_t tpa, const uint8_t *tha, const uint8_t *eth_hwaddr_dest)
{
  uint32_t local_ip_addr = 0;

  if (ipv4_local_address(&local_ip_addr) < 0)
    local_ip_addr = 0;

  header hdr;
  hdr.htype = htons(1);
  hdr.ptype = htons(ET_IPV4);
  hdr.hlen = 6;
  hdr.plen = 4;
  hdr.oper = htons(op);
  eth_hwaddr(fd, hdr.sha);
  hdr.spa = htonl(local_ip_addr);
  memcpy(hdr.tha, tha, 6);
  hdr.tpa = htonl(tpa);

  eth_frame frame = {
    .dest = eth_hwaddr_dest,
    .src = hdr.sha,
    .type = ET_ARP
  };

  return eth_send(fd, &frame, (char *)&hdr, sizeof(hdr));
}

// send_ipv4_request - broadcasts a request for @ipaddr
static void send_ipv4_request(int fd, uint32_t ipaddr)
{
  uint8_t tha[6] = {0, 0, 0, 0, 0, 0};
  uint8_t dest_hwaddr[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
  send_arp(fd, 1, ipaddr, tha, dest_hwaddr);
}

// send_ipv4_reply - sends our mapping to @ipaddr (@hwaddr)
static void send_ipv4_reply(int fd, uint32_t ipaddr, const uint8_t *hwaddr)
{
  send_arp(fd, 1, ipaddr, hwaddr, hwaddr);
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

static inline const cache_entry *fetch_cache_entry(uint32_t ipaddr)
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

static size_t find_ipv4_probe(uint32_t ipaddr)
{
  for (int i = 0; i < ipv4_probes.watermark(); ++i) {
    if (!ipv4_probes.valid(i))
      continue;

    if (ipv4_probes[i].key == ipaddr)
      return i;
  }

  return ipv4_probes.end_sentinel();
}
