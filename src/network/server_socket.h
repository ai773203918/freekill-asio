// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

using asio::ip::tcp;

class ClientSocket;

class ServerSocket {
public:
  ServerSocket() = delete;
  ServerSocket(ServerSocket &) = delete;
  ServerSocket(ServerSocket &&) = delete;
  ServerSocket(asio::io_context &io_ctx, tcp::endpoint end);

  void listen();

  // signal connectors
  void set_new_connection_callback(std::function<void(std::shared_ptr<ClientSocket>)>);

private:
  asio::io_context &io_ctx;
  tcp::acceptor m_acceptor;

  // signals
  std::function<void(std::shared_ptr<ClientSocket>)> new_connection_callback;

  /* TODO: udp
  QUdpSocket *udpSocket;

  void processDatagram(const QByteArray &msg, const QHostAddress &addr, uint port);
  void processNewConnection();
  void readPendingDatagrams();
  */
};
