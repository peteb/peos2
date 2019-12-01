#include "net/tcp_connection_table.h"
#include "utils.h"

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
  log(tcp_connection_table, "creating connection remote=%s:%d local=%s:%d",
      ipaddr_str(remote.ipaddr),
      remote.port,
      ipaddr_str(local.ipaddr),
      local.port);

  handle new_conn = _connections.emplace_back(_ipv4_if, this, remote, local, state);
  _connections[new_conn].handle = new_conn;
  _new_connections.push_back(new_conn);
  return new_conn;
}

void tcp_connection_table::finish_connection(handle conn_handle)
{
  _finished_connections.push_back(conn_handle);
}

void tcp_connection_table::tick(int dt)
{
  for (size_t i = 0; i < _connections.watermark(); ++i) {
    if (_connections.valid(i))
      _connections[i].tick(dt);
  }
}

tcp_connection_table::handle tcp_connection_table::end() const
{
  return _connections.end();
}

tcp_connection &tcp_connection_table::operator [](handle idx)
{
  return _connections[idx];
}

void tcp_connection_table::step_new_connections()
{
  for (size_t i = 0; i < _new_connections.watermark(); ++i) {
    if (_new_connections.valid(i)) {
      if (_connections.valid(_new_connections[i]))
        _connections[_new_connections[i]].step();

      _new_connections.erase(i);
    }
  }

  if (_new_connections.size() > 0) {
    log(tcp_connection_table, "warning, recursively creating connections");
  }
}

void tcp_connection_table::destroy_finished_connections()
{
  for (size_t i = 0; i < _finished_connections.watermark(); ++i) {
    if (_finished_connections.valid(i)) {
      handle handle_ = _finished_connections[i];

      if (!_connections.valid(handle_)) {
        log(tcp_connection_table, "double finish for handle %d", handle_);
        break;
      }

      log(tcp_connection_table, "removing connection");
      _connections.erase(handle_);
    }
  }

  _finished_connections.clear();
}
