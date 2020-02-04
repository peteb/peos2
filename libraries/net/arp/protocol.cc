#include <stdint.h>
#include <support/logging.h>

#include "arp/protocol.h"
#include "arp/definitions.h"
#include "ethernet/definitions.h"
#include "ethernet/utils.h"
#include "ipv4/utils.h"

#include "utils.h"
#include "protocol_stack.h"

namespace net::arp {

void protocol::on_receive(const net::ethernet::frame_metadata &metadata, const char *data, size_t length)
{
  header hdr;
  assert(length >= sizeof(hdr));
  memcpy(&hdr, data, sizeof(hdr));

  uint16_t htype{ntohs(hdr.htype)};
  net::ethernet::ether_type ptype{ntohs(hdr.ptype)};
  op oper{ntohs(hdr.oper)};

  if (htype != 1 || ptype != net::ethernet::ether_type::ET_IPV4) {
    log_debug("protocols not supported htype=%04x,ptype=%04x", htype, ptype);
    return;
  }

  if (hdr.hlen != 6 || hdr.plen != 4) {
    log_debug("address sizes not supported hlen=%02x,ptype=%02x", hdr.hlen, hdr.plen);
    return;
  }

  auto &local_hwaddr = _protocols.ethernet().hwaddr();

  if (memcmp(&local_hwaddr, hdr.sha, 6) == 0) {
    log_debug("ignoring message from ourselves");
    return;
  }

  net::ipv4::address spa{ntohl(hdr.spa)};
  net::ipv4::address tpa{ntohl(hdr.tpa)};

  using net::ethernet::hwaddr_str;
  using net::ipv4::ipaddr_str;

  if (oper == op::OP_REQUEST) {
    log_info("ARP who has %s (%s)? Tell %s (%s). sender=%s",
      ipaddr_str(tpa).c_str(),
      hwaddr_str(hdr.tha).c_str(),
      ipaddr_str(spa).c_str(),
      hwaddr_str(hdr.sha).c_str(),
      hwaddr_str(*metadata.mac_src).c_str());

    if (tpa == _protocols.ipv4()->local_address()) {
      // To avoid spamming the sender and the network, we only respond with data that we own and
      // never entries that we've learned
      send(op::OP_REPLY, spa, hdr.sha, *metadata.mac_src);
    }
    else {
      log_debug("received ARP request for other IP address");
    }
  }
  else if (oper == op::OP_REPLY) {
    log_info("ARP %s is at %s, sender=%s",
      ipaddr_str(spa).c_str(),
      hwaddr_str(hdr.sha).c_str(),
      hwaddr_str(*metadata.mac_src).c_str());

    write_cache_entry(spa, hdr.sha);
  }
  else {
    log_info("dropping message due to invalid oper=%04x", oper);
    return;
  }
}

int protocol::send(int op, net::ipv4::address tpa, const net::ethernet::address &tha, const net::ethernet::address &next_hop)
{
  header hdr;
  hdr.htype = htons(1);
  hdr.ptype = htons(net::ethernet::ET_IPV4);
  hdr.hlen = 6;
  hdr.plen = 4;
  hdr.oper = htons(op);

  // Sender address
  memcpy(hdr.sha, &_protocols.ethernet().hwaddr(), sizeof(hdr.sha));
  hdr.spa = htonl(_protocols.ipv4()->local_address());

  // Target address
  memcpy(hdr.tha, tha, 6);
  hdr.tpa = htonl(tpa);

  return _protocols.ethernet().send(net::ethernet::ET_ARP, next_hop, reinterpret_cast<const char *>(&hdr), sizeof(hdr));
}

void protocol::tick(uint32_t delta_ms)
{
  for (auto probe_it = _active_probes.begin(); probe_it != _active_probes.end(); ++probe_it) {
    if (!probe_it->value.tick(delta_ms)) {
      ipv4_lookup_result result = nullptr;
      probe_it->value.notify_all(result);
      _active_probes.erase(probe_it);
    }
  }
}

void protocol::write_cache_entry(net::ipv4::address ipaddr, net::ethernet::address hwaddr)
{
  _mappings.insert(ipaddr, hwaddr);

  if (auto probe_it = _active_probes.find(ipaddr); probe_it != _active_probes.end()) {
    // There's a probe waiting for this result
    ipv4_lookup_result result = &hwaddr;
    probe_it->value.notify_all(result);
    _active_probes.erase(probe_it);
  }
}

ipv4_lookup_result protocol::fetch_cached(net::ipv4::address ipaddr) const
{
  return _mappings.fetch(ipaddr);
}

void protocol::fetch_network(net::ipv4::address ipaddr, probe::await_fun callback)
{
  // Short-circuit in case we've already got the entry
  if (auto address = _mappings.fetch(ipaddr)) {
    callback(address);
    return;
  }

  if (auto it = _active_probes.find(ipaddr); it != _active_probes.end()) {
    // There's already a probe out for this ip address, let's bump its expiration so that it'll go on for a bit longer
    it->value.reset();
    it->value.await(callback);
  }
  else {
    // No probe already exists for this ipaddr, create one
    log_debug("creating probe for %s", net::ipv4::ipaddr_str(ipaddr));

    probe::op_fun operation = [=]() {
      send(op::OP_REQUEST, ipaddr, net::ethernet::address::wildcard, net::ethernet::address::broadcast);
    };

    _active_probes.insert(ipaddr, operation);

    // TODO: there's a lot of back-to-back lookups here. We need to be smarter about that.
    auto probe_it = _active_probes.find(ipaddr);
    assert(probe_it != _active_probes.end());
    probe_it->value.await(callback);
    probe_it->value.tick(0);
  }
}

void mapping_table::insert(net::ipv4::address ipaddr, net::ethernet::address hwaddr)
{
  if (_entries.full())
    _entries.pop_front();

  cache_entry entry;
  memcpy(&entry.hwaddr, hwaddr, sizeof(hwaddr));
  entry.ipaddr = ipaddr;
  _entries.push_back(entry);
}

const net::ethernet::address *mapping_table::fetch(net::ipv4::address ipaddr) const
{
  const net::ethernet::address *latest_value = nullptr;

  for (int index = _entries.begin(); index != _entries.end(); ++index) {
    if (_entries[index].ipaddr == ipaddr)
      latest_value = &_entries[index].hwaddr;
    // We need to iterate over the whole list as we might've received a new mapping for this ipaddr
  }

  return latest_value;
}


}