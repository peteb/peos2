#include "tcp_connection.h"
#include "utils.h"

namespace {
  int count_specific_matches(const tcp_endpoint &filter, const tcp_endpoint &endpoint)
  {
    int matching_fields = 0;

    if (filter.ipaddr != 0) {
      if (endpoint.ipaddr == filter.ipaddr)
        ++matching_fields;
      else
        return -1;
    }

    if (filter.port != 0) {
      if (endpoint.port == filter.port)
        ++matching_fields;
      else
        return -1;
    }

    return matching_fields;
  }
}


int tcp_connection::compare(const tcp_endpoint &remote, const tcp_endpoint &local)
{
  int remote_match = count_specific_matches(_remote, remote);
  int local_match = count_specific_matches(_local, local);

  if (remote_match >= 0 && local_match >= 0)
    return remote_match + local_match;
  else
    return -1;
}

void tcp_connection::recv(const tcp_segment &segment)
{
  assert(_state);

  _state->early_recv(*this, segment);

  char buffer[10 * 1024]; // Largest segment is 10K

  while (_rx_queue.has_readable()) {
    tcp_recv_segment ordered_segment;

    if (!_rx_queue.read_one_segment(&ordered_segment, buffer, sizeof(buffer))) {
      // This is weird, has_readable returned true...!
      log(tcp, "failed to read one segment");
      return;
    }

    _state->recv(*this, ordered_segment);
  }
}

void tcp_connection::rx_enqueue(const tcp_segment &segment)
{
  tcp_recv_segment ordered_segment;
  ordered_segment.flags = segment.tcphdr->flags_hdrsz & 0x1FF;
  ordered_segment.seqnbr = segment.tcphdr->seq_nbr;
  _rx_queue.insert(ordered_segment, segment.payload, segment.payload_size);
  // TODO: handle result? What if the queue is full?
}
