// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <openssl/aes.h>

using asio::ip::tcp;

class ClientSocket {

public:
  ClientSocket() = delete;
  ClientSocket(ClientSocket &) = delete;
  ClientSocket(ClientSocket &&) = delete;
  explicit ClientSocket(tcp::socket socket);

  void start();

  /*
  void disconnectFromHost();
  void installAESKey(const QByteArray &key);
  void removeAESKey();
  bool aesReady() const { return aes_ready; }
  void send(const QByteArray& msg);
  bool isConnected() const;
  QString peerName() const;
  QString peerAddress() const;
  QTimer timerSignup;

signals:
  void message_got(const QCborArray &msg);
  void error_message(const QString &msg);
  void disconnected();
  void connected();
  */

private:
  tcp::socket m_socket;

  void wait_for_message();

  /*
  QByteArray aesEnc(const QByteArray &in);
  QByteArray aesDec(const QByteArray &out);
  void init();

  AES_KEY aes_key;
  bool aes_ready;

  QByteArray cborBuffer;

  void getMessage();
  void raiseError(QAbstractSocket::SocketError error);

  QList<QCborArray> readCborArrsFromBuffer(QCborError *err);
  */
};
