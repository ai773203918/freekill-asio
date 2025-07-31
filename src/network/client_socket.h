// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <openssl/aes.h>

using asio::ip::tcp;

// 为了省那几字节重排了一下字段
// 实际应当是 `[ reqId, type, command, data, timeout, timestamp ]`

// Router负责处理的东西
struct Packet {
  int requestId;
  int type;
  int timeout;
  int _len;
  int64_t timestamp;
  std::string_view command;
  std::string_view cborData;

  Packet() = default;
  Packet(Packet &) = delete;
  Packet(Packet &&) = delete;

  void describe();
};

class ClientSocket : public std::enable_shared_from_this<ClientSocket> {
public:
  ClientSocket() = delete;
  ClientSocket(ClientSocket &) = delete;
  ClientSocket(ClientSocket &&) = delete;
  explicit ClientSocket(tcp::socket socket);

  void start();

  tcp::socket &socket();
  std::string_view peerAddress() const;

  void disconnectFromHost();
  void send(const asio::const_buffer &msg);

  // signal connectors
  void set_disconnected_callback(std::function<void()>);
  void set_message_got_callback(std::function<void(Packet &)>);

  /*
  void installAESKey(const QByteArray &key);
  void removeAESKey();
  bool aesReady() const { return aes_ready; }
  bool isConnected() const;
  QTimer timerSignup;
  */

private:
  tcp::socket m_socket;
  enum { max_length = 32768 };
  char m_data[max_length];

  std::string m_peer_address;

  std::vector<unsigned char> cborBuffer;

  bool handleBuffer(size_t length);

  // signals
  std::function<void()> disconnected_callback = 0;
  std::function<void(Packet &)> message_got_callback = 0;

  /*
  QByteArray aesEnc(const QByteArray &in);
  QByteArray aesDec(const QByteArray &out);
  void init();

  AES_KEY aes_key;
  bool aes_ready;
  */
};
