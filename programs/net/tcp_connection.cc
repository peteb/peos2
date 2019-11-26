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

static const class : public tcp_connection_state {
  void recv(const tcp_segment &segment) const {
    (void)segment;
    log(tcp, "rx segment in LISTEN");
  }
} listen;
const tcp_connection_state *tcp_connection_state::LISTEN = &listen;

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
  (void)segment;
  if (_state) {
    _state->recv(segment);
  }
  // 1. check if we should kill this segment early: ie, if the
  // sequence number is larger than read cursor + window size. BUT if SYN is set, skip

  //!!! how can we write SYNs into our rx buffer? we can't. so we should just sen an ACK immediately
}
