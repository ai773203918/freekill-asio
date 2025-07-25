// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/server.h"

int main(int argc, char *argv[]) {
  try {
    asio::io_context io_ctx;

    auto &server = Server::instance();
    server->listen( io_ctx, tcp::endpoint(tcp::v6(), 9927u) );

    io_ctx.run();
  } catch (std::exception e) {
    spdlog::error(e.what());
  }
}
