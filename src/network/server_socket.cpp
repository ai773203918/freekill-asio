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
      spdlog::info("New client connected from {}",
                   conn->peerAddress());

      conn->start();
    } else {
      spdlog::error("Accept error: {}", err.message());
    }

    listen();
  });
}


/**
void ServerSocket::processNewConnection() {
  QTcpSocket *socket = server->nextPendingConnection();
  ClientSocket *connection = new ClientSocket(socket);
  // 这里怎么能一断连就自己删呢，应该让上层的来
  // connect(connection, &ClientSocket::disconnected, this,
  //        [connection]() { connection->deleteLater(); });
  emit new_connection(connection);
}
*/
