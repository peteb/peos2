#include <support/logging.h>

#include "tcp/connection_table.h"
#include "utils.h"
#include "../utils.h"

namespace net::tcp {
  connection_table::connection_table(net::ipv4::protocol *ipv4)
    : _ipv4(ipv4)
  {
  }

  connection_table::handle connection_table::find_best_match(const endpoint &remote,
                                                            const endpoint &local)
  {
    int most_specific_match = -1;
    auto most_specific_it = _connections.end();

    for (auto it = _connections.begin(); it != _connections.end(); ++it) {
      int score = it->compare(remote, local);

      if (score > most_specific_match) {
        most_specific_match = score;
        most_specific_it = it;
      }
    }

    if (most_specific_it == _connections.end())
      return _connections.end_sentinel();
    else
      return most_specific_it.index();
  }

  connection_table::handle connection_table::create_connection(const endpoint &remote,
                                                              const endpoint &local,
                                                              const connection_state *state)
  {
    log_info("creating connection remote=%s:%d local=%s:%d",
        ipaddr_str(remote.ipaddr),
        remote.port,
        ipaddr_str(local.ipaddr),
        local.port);

    if (_connections.full()) {
      log(tcp_connection_table, "connection table is full!");
      // TODO: don't crash when the table is full
    }

    handle connection_id = _connections.emplace_anywhere(_ipv4, *this, remote, local, state);
    connection &new_connection = _connections[connection_id];
    new_connection.handle = connection_id;
    new_connection.set_callback(_callback);
    _new_connections.emplace_anywhere(connection_id);
    return connection_id;
  }

  void connection_table::finish_connection(handle conn_handle)
  {
    _finished_connections.emplace_anywhere(conn_handle);
  }

  void connection_table::tick(int dt)
  {
    for (auto &connection : _connections) {
      connection.tick(dt);
    }
  }

  connection_table::handle connection_table::end() const
  {
    return _connections.end_sentinel();
  }

  connection &connection_table::operator [](handle idx)
  {
    return _connections[idx];
  }

  void connection_table::step_new_connections()
  {
    for (auto &connection_handle : _new_connections) {
      if (_connections.valid(connection_handle)) {
        log(tcp, "stepping new connection %d", connection_handle);

        size_t new_connection_count = _new_connections.size();
        _connections[connection_handle].step();

        if (_new_connections.size() != new_connection_count) {
          log(tcp_connection_table, "warning, recursively creating connections");
        }
      }
    }

    _new_connections.clear();
  }

  void connection_table::destroy_finished_connections()
  {
    for (auto &conn_handle : _finished_connections) {
      if (!_connections.valid(conn_handle)) {
        log(tcp_connection_table, "double finish for handle %d", conn_handle);
        break;
      }

      log(tcp_connection_table, "removing connection");
      _connections.erase(conn_handle);
    }

    _finished_connections.clear();
  }
}
