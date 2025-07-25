// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/server_socket.h"

int main(int argc, char *argv[]) {
  try {
    asio::io_context io_ctx;
    ServerSocket server { io_ctx, tcp::endpoint(tcp::v6(), 9927u) };
    io_ctx.run();
  } catch (std::exception e) {
    spdlog::error(e.what());
  }
}
