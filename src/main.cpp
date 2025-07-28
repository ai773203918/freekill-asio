// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/server.h"
#include "server/gamelogic/roomthread.h"

int main(int argc, char *argv[]) {
  asio::io_context io_ctx;

  auto &server = Server::instance();
  server.listen( io_ctx, tcp::endpoint(tcp::v6(), 9527u) );

  auto thr = server.getThread(0);
  if (thr) {
    thr->quit();
  }

  io_ctx.run();
}
