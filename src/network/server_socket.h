// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

using asio::ip::tcp;

class ClientSocket;

class ServerSocket {
public:
  ServerSocket(asio::io_context &io_ctx, tcp::endpoint end);

  /*

  // signal new_connection
  std::function<void(ClientSocket *socket)> new_connection;
  */

private:
  tcp::acceptor m_acceptor;

  void listen();

  /*
  QUdpSocket *udpSocket;

  void processDatagram(const QByteArray &msg, const QHostAddress &addr, uint port);
  void processNewConnection();
  void readPendingDatagrams();
  */
};
