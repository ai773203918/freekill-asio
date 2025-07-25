// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/server_socket.h"
#include "network/client_socket.h"

ServerSocket::ServerSocket(asio::io_context &io_ctx, tcp::endpoint end):
  m_acceptor { io_ctx, end }
{
  spdlog::info("server listen on {}", end.port());
  listen();
}

void ServerSocket::listen() {
  m_acceptor.async_accept([&](std::error_code ec, tcp::socket socket) {
    if (!ec) {
      spdlog::info("New client connected from [{}]:{}",
                   socket.remote_endpoint().address().to_string(),
                   socket.remote_endpoint().port());

      // 创建新会话并启动
      auto sock = std::make_shared<ClientSocket>(std::move(socket));
    } else {
      spdlog::error("Accept error: {}", ec.message());
    }

    // 继续接受新的连接
    listen();
  });
}

/**
void ServerSocket::processNewConnection() {
  QTcpSocket *socket = server->nextPendingConnection();
  ClientSocket *connection = new ClientSocket(socket);
  // 这里怎么能一断连就自己删呢，应该让上层的来
  //connect(connection, &ClientSocket::disconnected, this,
  //        [connection]() { connection->deleteLater(); });
  emit new_connection(connection);
}
*/
