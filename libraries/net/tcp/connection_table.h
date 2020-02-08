// -*- c++ -*-

#pragma once

#include <support/pool.h>
#include <support/optional.h>

#include "tcp/connection.h"
#include "ipv4/definitions.h"

namespace net::ipv4 {
  class protocol;
}

namespace net::tcp {
  class callback;
}

namespace net::tcp {
  class connection_table {
  public:
    connection_table(net::ipv4::protocol *ipv4);

    using handle = uint16_t;

    handle find_best_match(const endpoint &remote,
                           const endpoint &local);

    handle create_connection(const endpoint &remote,
                             const endpoint &local,
                             const connection_state *state);

    void finish_connection(handle);

    void tick(int dt);
    void step_new_connections();
    void destroy_finished_connections();

    handle end() const;
    connection &operator [](handle idx);

    void set_callback(net::tcp::callback *callback_) {_callback = callback_; }

  private:
    net::ipv4::protocol *_ipv4;
    p2::fixed_pool<connection, 40, handle> _connections;
    p2::fixed_pool<handle, 10> _new_connections, _finished_connections;
    net::tcp::callback *_callback = nullptr;
  };

}
