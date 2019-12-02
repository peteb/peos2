// -*- c++ -*-

#ifndef NET_TCP_CONNECTION_TABLE_H
#define NET_TCP_CONNECTION_TABLE_H

#include <support/pool.h>
#include <support/optional.h>

#include "tcp_connection.h"

class tcp_connection_table {
public:
  typedef uint16_t handle;

  handle find_best_match(const tcp_endpoint &remote, const tcp_endpoint &local);
  handle create_connection(const tcp_endpoint &remote, const tcp_endpoint &local, const tcp_connection_state *state);
  void finish_connection(handle);

  void tick(int dt);
  void step_new_connections();
  void destroy_finished_connections();

  handle end() const;
  tcp_connection &operator [](handle idx);

  void set_interface(int interface) {_interface = interface; }
  int interface() {return _interface; }
  void set_ipv4(ipv4_interface *ipv4_if) {_ipv4_if = ipv4_if; }

private:
  int _interface = -1;
  p2::pool<tcp_connection, 40, handle> _connections;
  p2::pool<handle, 10> _new_connections, _finished_connections;
  ipv4_interface *_ipv4_if = nullptr;
};

#endif // !NET_TCP_CONNECTION_TABLE_H
