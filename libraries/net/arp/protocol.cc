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

  uint16_t htype = ntohs(hdr.htype);
  net::ethernet::ether_type ptype{ntohs(hdr.ptype)};
  op oper = op{ntohs(hdr.oper)};

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

    if (tpa == _protocols.ipv4().local_address()) {
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
      net::ethernet::hwaddr_str(hdr.sha).c_str(),
      net::ethernet::hwaddr_str(*metadata.mac_src).c_str());

    /*
    add_cache_entry(spa, hdr.sha);

    if (size_t probe_idx = find_ipv4_probe(spa); probe_idx != ipv4_probes.end_sentinel()) {
      ipv4_probes[probe_idx].value.notify_all(PROBE_REPLY);
      ipv4_probes.erase(probe_idx);
    }*/
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
  hdr.spa = htonl(_protocols.ipv4().local_address());

  // Target address
  memcpy(hdr.tha, tha, 6);
  hdr.tpa = htonl(tpa);

  return _protocols.ethernet().send(net::ethernet::ET_ARP, next_hop, reinterpret_cast<const char *>(&hdr), sizeof(hdr));
}

}