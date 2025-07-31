// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/server_socket.h"
#include "network/client_socket.h"

ServerSocket::ServerSocket(asio::io_context &io_ctx, tcp::endpoint end):
  io_ctx { io_ctx }, m_acceptor { io_ctx, end }
{
  spdlog::info("server is ready to listen on {}", end.port());
}

void ServerSocket::listen() {
  m_acceptor.async_accept([this](const asio::error_code &err, tcp::socket socket) {
    if (!err) {
      auto conn = std::make_shared<ClientSocket>(std::move(socket));

      if (new_connection_callback) {
        new_connection_callback(conn);
        conn->start();
      }
    } else {
      spdlog::error("Accept error: {}", err.message());
    }

    listen();
  });
}

void ServerSocket::set_new_connection_callback(std::function<void(std::shared_ptr<ClientSocket>)> f) {
  new_connection_callback = f;
}

