#include <support/logging.h>
#include <support/utils.h>

#include "ipv4/protocol.h"
#include "ipv4/utils.h"
#include "ethernet/utils.h"
#include "ethernet/definitions.h"

#include "protocol_stack.h"
#include "../utils.h"

namespace {
  bool forward_datagram(net::protocol_stack &protocols, const net::ipv4::datagram_metadata &metadata, net::ipv4::proto protocol, const char *data, size_t length)
  {
    switch (protocol) {
    case net::ipv4::proto::PROTO_TCP:
      protocols.tcp().on_receive(metadata, data, length);
      return true;

    case net::ipv4::proto::PROTO_UDP:
      protocols.udp().on_receive(metadata, data, length);
      return true;

    case net::ipv4::proto::PROTO_ICMP:
      protocols.icmp().on_receive(metadata, data, length);
      return true;

    default:
      return false;
    }
  }

  bool is_broadcast_address(net::ipv4::address addr, net::ipv4::address netmask)
  {
    return (addr & ~netmask) == (net::ipv4::address{-1u} & ~netmask);
  }

  bool is_same_subnet(net::ipv4::address addr1, net::ipv4::address addr2, net::ipv4::address netmask)
  {
    return (addr1 & netmask) == (addr2 & netmask);
  }
}

namespace net::ipv4 {
  protocol_impl::protocol_impl(protocol_stack &protocols)
    : _protocols(protocols)
  {
  }

  void protocol_impl::on_receive(const net::ethernet::frame_metadata &, const char *data, size_t length)
  {
    header hdr;
    assert(length > sizeof(hdr));
    memcpy(&hdr, data, sizeof(hdr));

    uint16_t total_len = ntohs(hdr.total_len);
    uint16_t id = ntohs(hdr.id);
    uint16_t frag_ofs = ntohs(hdr.frag_ofs);
    uint16_t checksum = ntohs(hdr.checksum);
    address src_addr = ntohl(hdr.src_addr);
    address dest_addr = ntohl(hdr.dest_addr);
    size_t header_length = hdr.ihl * 4;

    uint8_t flags = frag_ofs >> 12;
    uint16_t frag_offset = (frag_ofs & 0x1FFF) * 8;

    log_debug("rx packet src=%s,dest=%s,total_len=%d,id=%d,frag_ofs=%d,flags=%02x,checksum=%04x",
      net::ipv4::ipaddr_str(src_addr), net::ipv4::ipaddr_str(dest_addr), total_len, id, frag_offset, flags, checksum);

    if (hdr.version != 4) {
      log_debug("dropping packet due to incorrect version");
      return;
    }

    if (header_length > 60) {
      log_debug("dropping packet due to header too large");
      return;
    }

    if (total_len < header_length) {
      log_debug("dropping packet due to total datagram length is less than the header length");
      return;
    }

    if (total_len > length) {
      log_debug("dropping packet due to total length being larger than the packet");
      return;
    }

    if (hdr.ttl == 0) {
      log_debug("dropping packet due to ttl = 0");
      return;
    }

    const char *options = nullptr;
    size_t options_length = 0;

    if (header_length > sizeof(hdr)) {
      options = data + sizeof(hdr);
      options_length = header_length - sizeof(hdr);
    }

    hdr.checksum = 0;
    uint16_t calculated_checksum = htons(net::ipv4::checksum(hdr, options, options_length));
    // htons due to checksuming over network byte order rather than
    // host byte order like in #send

    if (checksum != calculated_checksum) {
      log_debug("dropping packet due to incorrect checksum, calculated=%04x, received=%04x", calculated_checksum, checksum);
      return;
    }

    if (!is_recipient(dest_addr)) {
      log_debug("dropping packet due to not being the intended receiver, dest_addr=%s", net::ipv4::ipaddr_str(dest_addr).c_str());
      return;
    }

    const char *payload = data + 4 * hdr.ihl; // TODO: check that this isn't OOB
    size_t payload_size = p2::min<unsigned>(total_len - 4 * hdr.ihl, length - sizeof(hdr)); // TODO: add options

    datagram_metadata ipv4_metadata;
    ipv4_metadata.src_addr = src_addr;
    ipv4_metadata.dest_addr = dest_addr;

    if (!(flags & flags::FLAGS_MF) && hdr.frag_ofs == 0) {
      // All data was contained in this packet, so we can just forward it
      if (!forward_datagram(_protocols, ipv4_metadata, proto{hdr.protocol}, payload, payload_size)) {
        log_debug("dropping packet due to unsupported protocol");
        return;
      }
    }
    else {
      if (_reassembly_buffers.full()) {
        log_warn("dropping packet due to no free reassembly buffers");
        return;
      }

      reassembly_buffer_identifier ident{dest_addr, src_addr, id, hdr.protocol};
      reassembly_buffer *buffer = nullptr;
      auto buffer_it = _reassembly_buffers.find(ident);

      if (buffer_it != _reassembly_buffers.end()) {
        buffer = &buffer_it->value;
      }
      else {
        _reassembly_buffers.insert(ident, {});
        buffer_it = _reassembly_buffers.find(ident);  // TODO: maybe insert should return the iterator instead
        assert(buffer_it != _reassembly_buffers.end());
        buffer = &buffer_it->value;
      }

      assert(buffer);

      if (!(flags & flags::FLAGS_MF)) {
        // This is the last fragment
        buffer->received_last = true;
      }

      buffer->data.insert(frag_offset, payload, payload_size);

      if (buffer->received_last && buffer->data.continuous_size() > 0) {
        // We've got a complete datagram
        forward_datagram(_protocols,
                         ipv4_metadata,
                         proto{hdr.protocol},
                         (const char *)buffer->data.data(),
                         buffer->data.continuous_size());
        _reassembly_buffers.erase(buffer_it);
      }
    }
  }

  bool protocol_impl::send(net::ipv4::proto protocol, net::ipv4::address destination, const char *data, size_t length)
  {
    net::ipv4::address next_hop_ipaddr{};

    if ((_local & _netmask) == (destination & _netmask)) {
      // Same subnet; just send it directly to the recipient
      next_hop_ipaddr = destination;
    }
    else {
      // Different subnet; we need to route it through our default gateway
      next_hop_ipaddr = _default_gateway;
    }

    if (auto next_hop_hwaddr = _protocols.arp().fetch_cached(next_hop_ipaddr)) {
      // We have a hwaddr for the next hop, so send immediately without allocating on buffer. This means that we can
      // send larger datagrams than what the _arp_wait_buffer can hold.
      send(protocol, destination, *next_hop_hwaddr, data, length);
      return true;
    }
    else {
      if (length > _arp_wait_buffer.capacity) {
        log_warn("datagram is too large to fit in ARP lookup waiting queue, try again later. length=%d", length);

        // Issue the lookup so that there's a chance that it might work on retry
        _protocols.arp().fetch_network(next_hop_ipaddr, [](auto) {});
        return false;
      }

      auto data_handle = _arp_wait_buffer.alloc(length);
      memcpy(_arp_wait_buffer.data(data_handle), data, length);

      _protocols.arp().fetch_network(next_hop_ipaddr, [=](auto next_hop_hwaddr) {
        if (!next_hop_hwaddr) {
          log_info("dropping datagram due to ARP lookup failure");
          return;
        }

        char *payload = _arp_wait_buffer.data(data_handle);
        if (!payload) {
          log_info("dropping outgoing datagram due to payload buffer disappearing while waiting for ARP");
          return;
        }

        send(protocol, destination, *next_hop_hwaddr, payload, length);
      });
    }

    return true;
  }

  void protocol_impl::send(net::ipv4::proto protocol, net::ipv4::address destination, net::ethernet::address next_hop, const char *data, size_t length)
  {
    send_single_datagram(protocol, _local, destination, next_hop, data, length);
  }

  void protocol_impl::tick(uint32_t delta_ms)
  {
    for (auto iter = _reassembly_buffers.begin(), end_iter = _reassembly_buffers.end(); iter != end_iter; ) {
      if (iter->value.ttl <= 0) {
        ++iter;
        continue;
      }

      iter->value.ttl -= delta_ms;

      if (iter->value.ttl <= 0) {
        log_info("reassembly buffer for datagram is dropped due to timeout");
        _reassembly_buffers.erase(iter++);
      }
    }
  }

  void protocol_impl::configure(address local, address netmask, address default_gateway)
  {
    _local = local;
    _netmask = netmask;
    _default_gateway = default_gateway;

    log_info("configured with local=%s,netmask=%s,gateway=%s",
      net::ipv4::ipaddr_str(local).c_str(),
      net::ipv4::ipaddr_str(netmask).c_str(),
      net::ipv4::ipaddr_str(default_gateway).c_str());

    // Check that the default gateway is reachable. Also good to get the
    // gateway's MAC address in our cache
    _protocols.arp().fetch_network(default_gateway, [=](auto hwaddr) {
      if (!hwaddr) {
        log_error("failed to lookup default gateway");
        return;
      }

      auto gwy_hwaddr = _protocols.arp().fetch_cached(default_gateway);
      assert(gwy_hwaddr);
      log_info("default gateway located, hwaddr=%s", net::ethernet::hwaddr_str(*gwy_hwaddr).c_str());
    });
  }

  void protocol_impl::send_single_datagram(net::ipv4::proto protocol,
                                      net::ipv4::address src_addr,
                                      net::ipv4::address dest_addr,
                                      net::ethernet::address next_hop,
                                      const char *data,
                                      size_t length)
  {
    header hdr;
    hdr.ihl = 5; // 5 * 4 = 20 bytes
    hdr.version = 4;
    hdr.ecn_dscp = 0;
    hdr.total_len = htons(sizeof(hdr) + length);
    hdr.id = htons(_next_datagram_id++);  // TODO: according to the RFC, this can be unique per {src, dest, protocol}
    hdr.frag_ofs = htons(0);
    hdr.ttl = 65;
    hdr.protocol = protocol;
    hdr.checksum = 0;  // Field needs to be 0 when calculating the checksum
    hdr.src_addr = htonl(src_addr);
    hdr.dest_addr = htonl(dest_addr);
    hdr.checksum = net::ipv4::checksum(hdr, nullptr, 0);

    char buffer[1500];
    // TODO: fetch MTU from ethernet, but hard code to jumbo frames?

    if (sizeof(hdr) + length > sizeof(buffer)) {
      log_info("datagram too large for one packet, dropping");
      return;
    }

    memcpy(buffer, &hdr, sizeof(hdr));
    memcpy(buffer + sizeof(hdr), data, length);
    _protocols.ethernet().send(net::ethernet::ether_type::ET_IPV4, next_hop, buffer, sizeof(hdr) + length);
  }

  // Checks whether @dest_addr is an address that local host should respond to
  bool protocol_impl::is_recipient(net::ipv4::address dest_addr) const
  {
    if (dest_addr == _local)
      return true;

    if (is_same_subnet(dest_addr, _local, _netmask) &&
        is_broadcast_address(dest_addr, _netmask))
      return true;

    return false;
  }
}
