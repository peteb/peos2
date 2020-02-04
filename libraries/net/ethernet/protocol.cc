#include <support/utils.h>
#include <support/logging.h>

#include "ethernet/protocol.h"
#include "ethernet/utils.h"
#include "../utils.h"
#include "protocol_stack.h"

namespace net::ethernet {

protocol::protocol(protocol_stack &protocols)
  : _protocols(protocols)
{}

void protocol::on_receive(const char *data, size_t length)
{
  net::ethernet::header hdr;

  if (length < sizeof(hdr)) {
    log_warn("dropping received frame smaller than ethernet header, length=%d", length);
    return;
  }

  uint16_t pdu_size = length - sizeof(hdr);
  const char *pdu = data + sizeof(hdr);

  memcpy(&hdr, data, sizeof(hdr));
  net::ethernet::ether_type ether_type{ntohs(hdr.type)};

  log_debug("rx length=%d,pdusz=%d,type=%s,src=%s,dest=%s",
      length,
      pdu_size,
      ether_type_str(ether_type).c_str(),
      hwaddr_str(hdr.mac_src).c_str(),
      hwaddr_str(hdr.mac_dest).c_str());

  frame_metadata metadata;
  metadata.mac_src = &hdr.mac_src;
  metadata.mac_dest = &hdr.mac_dest;

  switch (ether_type) {
  case ET_ARP:
    _protocols.arp().on_receive(metadata, pdu, pdu_size);
    break;

  case ET_IPV4:
    _protocols.ipv4()->on_receive(metadata, pdu, pdu_size);
    break;

  default:
    log_debug("dropping packet due to unsupported type");
  }
}

int protocol::send(ether_type type, const address &destination, const char *data, size_t length)
{
  log_debug("tx pdusz=%d,dst=%s", length, hwaddr_str(destination).c_str());

  // TODO: looping writes
  header hdr;
  memcpy(hdr.mac_dest, &destination, 6);
  memcpy(hdr.mac_src, &_hwaddr, 6);
  hdr.type = htons(type);

  char frame[5128]; // TODO: correct size
  assert(sizeof(hdr) + length <= sizeof(frame));
  memcpy(frame, &hdr, sizeof(hdr));
  memcpy(frame + sizeof(hdr), data, length);

  return _protocols.device().send(frame, sizeof(hdr) + length);
}

void protocol::configure(const address &hwaddr)
{
  memcpy(&_hwaddr, &hwaddr, sizeof(hwaddr));
  log_info("ethernet: configured with hwaddr=%s", hwaddr_str(hwaddr).c_str());
}

}