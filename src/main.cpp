// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/server.h"

int main(int argc, char *argv[]) {
  asio::io_context io_ctx;

  auto &server = Server::instance();
  server.listen( io_ctx, tcp::endpoint(tcp::v6(), 9527u) );

  io_ctx.run();
}
