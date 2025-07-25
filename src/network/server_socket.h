// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class ClientSocket;

class ServerSocket {

public:
  ServerSocket(QObject *parent = nullptr);

  bool listen(const QHostAddress &address = QHostAddress::Any, ushort port = 9527u);

signals:
  void new_connection(ClientSocket *socket);

private slots:
  void processNewConnection();
  void readPendingDatagrams();

private:
  QTcpServer *server;
  QUdpSocket *udpSocket;

  void processDatagram(const QByteArray &msg, const QHostAddress &addr, uint port);
};
