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

  // signal connectors
  void set_disconnected_callback(std::function<void()>);
  void set_message_got_callback(std::function<void(cbor_item_t *)>);

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
  */

private:
  tcp::socket m_socket;

  void wait_for_message();

  std::vector<unsigned char> cborBuffer;
  std::vector<cbor_item_t> readCborArrsFromBuffer(cbor_error *err);

  // signals
  std::function<void()> disconnected_callback;
  std::function<void(cbor_item_t *)> message_got_callback;

  /*
  QByteArray aesEnc(const QByteArray &in);
  QByteArray aesDec(const QByteArray &out);
  void init();

  AES_KEY aes_key;
  bool aes_ready;

  void getMessage();
  void raiseError(QAbstractSocket::SocketError error);
  */
};
