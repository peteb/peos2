#include "net/tcp_connection_table.h"

tcp_connection_table::handle tcp_connection_table::find_best_match(const tcp_endpoint &remote,
                                                                   const tcp_endpoint &local)
{
  int most_specific_match = -1;
  tcp_connection_table::handle most_specific = _connections.end();

  for (size_t i = 0; i < _connections.watermark(); ++i) {
    if (!_connections.valid(i))
      continue;

    int score = _connections[i].compare(remote, local);

    if (score > most_specific_match) {
      most_specific_match = score;
      most_specific = i;
    }
  }

  if (most_specific == _connections.end())
    return {};
  else
    return most_specific;
}


tcp_connection_table::handle tcp_connection_table::create_connection(const tcp_endpoint &remote,
                                                                     const tcp_endpoint &local,
                                                                     const tcp_connection_state *state)
{
  return _connections.emplace_back(remote, local, state);
}

tcp_connection_table::handle tcp_connection_table::end() const
{
  return _connections.end();
}

tcp_connection &tcp_connection_table::operator [](handle idx)
{
  return _connections[idx];
}
