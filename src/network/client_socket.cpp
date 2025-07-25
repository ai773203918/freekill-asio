// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/client_socket.h"
#include <openssl/aes.h>

ClientSocket::ClientSocket(tcp::socket socket) : m_socket(std::move(socket)) {
  m_peer_address = m_socket.remote_endpoint().address().to_string();
  disconnected_callback = [this]() {
    spdlog::info("client {} disconnected", peerAddress());
  };

  message_got_callback = [this](cbor_item_t *item) {
    spdlog::info("cbor item {} got", (void *)item);
    cbor_describe(item, stdout);
  };
}

void ClientSocket::start() {
  auto self { shared_from_this() };
  m_socket.async_read_some(
    asio::buffer(m_data, max_length),
    [this, self](asio::error_code err, std::size_t length) {
      if (!err) {
        if (getMessage(length)) {
          // 再次read_some
          start();
        } else {
          spdlog::warn("Malfromed data from client {}", peerAddress());
          disconnected_callback();
        }
      } else {
        disconnected_callback();
      }
    }
  );
}

tcp::socket &ClientSocket::socket() {
  return m_socket;
}

std::string_view ClientSocket::peerAddress() const {
  return m_peer_address;
}

void ClientSocket::disconnectFromHost() {
  m_socket.close();
  // asio::post(m_socket.get_executor(), [this]() {
  //   asio::error_code ec;
  //   m_socket.shutdown(tcp::socket::shutdown_both, ec);
  //   m_socket.close(ec);
  // });
}

// 此函数必须只能在主线程调用！
void ClientSocket::send(const asio::const_buffer &msg) {
  asio::async_write(m_socket, msg, [](const asio::error_code &, std::size_t) {
    // no-op
  });
}

void ClientSocket::set_disconnected_callback(std::function<void()> f) {
  disconnected_callback = f;
}

void ClientSocket::set_message_got_callback(std::function<void(cbor_item_t *)> f) {
  message_got_callback = f;
}

// private methods

bool ClientSocket::getMessage(std::size_t length) {
  cborBuffer.insert(cborBuffer.end(), m_data, m_data + length);

  cbor_error err;
  auto arr = readCborArrsFromBuffer(&err);
  if (err.code == CBOR_ERR_NOTENOUGHDATA || err.code == CBOR_ERR_NODATA) {
    for (auto &a : arr) {
      message_got_callback(a);
      cbor_decref(&a);
    }
    return true;
  } else {
    // TODO: close conn?
    // 反正肯定会有不合法数据的，比如invalid setup string
    // 旧版客户端啥的
    // disconnectFromHost();
    return false;
  }
}

std::vector<cbor_item_t *> ClientSocket::readCborArrsFromBuffer(cbor_error *err) {
  auto cbuf = (unsigned char *)cborBuffer.data();
  auto len = cborBuffer.size();
  std::vector<cbor_item_t *> ret;
  size_t total_consumed = 0;

  static struct cbor_callbacks callbacks = cbor_empty_callbacks;
  struct cbor_decoder_result decode_result;

  while (true) {
    struct cbor_load_result result;
    // 尝试解析CBOR数据
    cbor_item_t *item = cbor_load(cbuf + total_consumed,
                                  len - total_consumed,
                                  &result);
    cbor_stream_decode(cbuf, len, &callbacks, nullptr);

    // 处理解析结果
    if (result.error.code == CBOR_ERR_NONE) {
      if (cbor_isa_array(item)) {
        ret.push_back(item);
        total_consumed += result.read;
      } else {
        // 不是数组，停止解析
        cbor_decref(&item);
        break;
      }
    } else if (result.error.code == CBOR_ERR_NOTENOUGHDATA) {
      // 数据不完整，保留剩余数据
      err->code = result.error.code;
      break;
    } else {
      // 其他错误，停止解析
      err->code = result.error.code;
      break;
    }
  }

  // 对剩余的不全数据深拷贝 重新造bytes
  std::vector<char> remaining_buffer;
  if (total_consumed < len) {
    remaining_buffer.assign(cborBuffer.begin() + total_consumed, cborBuffer.end());
    cborBuffer = remaining_buffer;
  }

  return ret;
}


/*
ClientSocket::ClientSocket(QTcpSocket *socket) {
  aes_ready = false;
  socket->setParent(this);
  this->socket = socket;
  timerSignup.setSingleShot(true);
  connect(&timerSignup, &QTimer::timeout, this,
          &ClientSocket::disconnectFromHost);
  connect(&timerSignup, &QTimer::timeout, this, &QObject::deleteLater);
  init();
}

void ClientSocket::init() {
  connect(socket, &QTcpSocket::connected, this, &ClientSocket::connected);
  connect(socket, &QTcpSocket::disconnected, this, &ClientSocket::disconnected);
  connect(socket, &QTcpSocket::disconnected, this, &ClientSocket::removeAESKey);
  connect(socket, &QTcpSocket::readyRead, this, &ClientSocket::getMessage);
  connect(socket, &QTcpSocket::errorOccurred, this, &ClientSocket::raiseError);
  socket->setSocketOption(QAbstractSocket::KeepAliveOption, 1);
}

void ClientSocket::connectToHost(const QString &address, ushort port) {
  socket->connectToHost(address, port);
}

void ClientSocket::getMessage() {
  cborBuffer += socket->readAll();
  QCborError err;
  auto arr = readCborArrsFromBuffer(&err);
  // 因为在读TcpSocket，必定读到EOF错误，上面那个函数是无限的读下去的
  if (err == QCborError::EndOfFile) {
    for (auto &a : arr) emit message_got(a);
    return;
  } else {
    // TODO: close conn?
    // 反正肯定会有不合法数据的，比如invalid setup string
    // 旧版客户端啥的
    disconnectFromHost();
    return;
  }
  // while (socket->canReadLine()) {
  //   auto msg = socket->readLine();
  //   msg = aesDec(msg);
  //   if (msg.startsWith("Compressed")) {
  //     msg = msg.sliced(10);
  //     msg = qUncompress(QByteArray::fromBase64(msg));
  //   }
  //   emit message_got(msg.simplified());
  // }
}

void ClientSocket::disconnectFromHost() {
  aes_ready = false;
  socket->disconnectFromHost();
}

void ClientSocket::send(const QByteArray &msg) {
  if (socket->state() != QTcpSocket::ConnectedState) {
    emit error_message("Cannot send messages if not connected");
    return;
  }
  // QByteArray _msg;
  // if (msg.length() >= 1024) {
  //   auto comp = qCompress(msg);
  //   _msg = QByteArrayLiteral("Compressed") + comp.toBase64();
  //   _msg = aesEnc(_msg) + "\n";
  // } else {
  //   _msg = aesEnc(msg) + "\n";
  // }

  socket->write(msg);
  socket->flush();
}

bool ClientSocket::isConnected() const {
  return socket->state() == QTcpSocket::ConnectedState;
}

QString ClientSocket::peerName() const {
  QString peer_name = socket->peerName();
  if (peer_name.isEmpty())
    peer_name = QString("%1:%2")
                    .arg(socket->peerAddress().toString())
                    .arg(socket->peerPort());

  return peer_name;
}

QString ClientSocket::peerAddress() const {
  return socket->peerAddress().toString();
}

void ClientSocket::raiseError(QAbstractSocket::SocketError socket_error) {
  // translate error message
  QString reason;
  switch (socket_error) {
  case QAbstractSocket::ConnectionRefusedError:
    reason = tr("Connection was refused or timeout");
    break;
  case QAbstractSocket::RemoteHostClosedError:
    reason = tr("Remote host close this connection");
    break;
  case QAbstractSocket::HostNotFoundError:
    reason = tr("Host not found");
    break;
  case QAbstractSocket::SocketAccessError:
    reason = tr("Socket access error");
    break;
  case QAbstractSocket::SocketResourceError:
    reason = tr("Socket resource error");
    break;
  case QAbstractSocket::SocketTimeoutError:
    reason = tr("Socket timeout error");
    break;
  case QAbstractSocket::DatagramTooLargeError:
    reason = tr("Datagram too large error");
    break;
  case QAbstractSocket::NetworkError:
    reason = tr("Network error");
    break;
  case QAbstractSocket::UnsupportedSocketOperationError:
    reason = tr("Unsupprted socket operation");
    break;
  case QAbstractSocket::UnfinishedSocketOperationError:
    reason = tr("Unfinished socket operation");
    break;
  case QAbstractSocket::ProxyAuthenticationRequiredError:
    reason = tr("Proxy auth error");
    break;
  case QAbstractSocket::ProxyConnectionRefusedError:
    reason = tr("Proxy refused");
    break;
  case QAbstractSocket::ProxyConnectionClosedError:
    reason = tr("Proxy closed");
    break;
  case QAbstractSocket::ProxyConnectionTimeoutError:
    reason = tr("Proxy timeout");
    break;
  case QAbstractSocket::ProxyProtocolError:
    reason = tr("Proxy protocol error");
    break;
  case QAbstractSocket::OperationError:
    reason = tr("Operation error");
    break;
  case QAbstractSocket::TemporaryError:
    reason = tr("Temporary error");
    break;
  default:
    reason = tr("Unknown error");
    break;
  }

  emit error_message(tr("Connection failed, error code = %1\n reason: %2")
                         .arg(socket_error)
                         .arg(reason));
}

void ClientSocket::installAESKey(const QByteArray &key) {
  if (key.length() != 32) {
    return;
  }
  auto key_ = QByteArray::fromHex(key);
  if (key_.length() != 16) {
    return;
  }

  AES_set_encrypt_key((const unsigned char *)key_.data(), 16 * 8, &aes_key);
  aes_ready = true;
}

void ClientSocket::removeAESKey() {
  aes_ready = false;
}

QByteArray ClientSocket::aesEnc(const QByteArray &in) {
  if (!aes_ready) {
    return in;
  }
  int num = 0;
  QByteArray out;
  out.resize(in.length());

  static auto rand_generator = QRandomGenerator::securelySeeded();
  static QByteArray iv_raw(16, Qt::Uninitialized);

  rand_generator.fillRange(reinterpret_cast<quint32*>(iv_raw.data()), 4);

  static unsigned char tempIv[16];
  strncpy((char *)tempIv, iv_raw.constData(), 16);
  AES_cfb128_encrypt((const unsigned char *)in.constData(),
                     (unsigned char *)out.data(), in.length(), &aes_key, tempIv,
                     &num, AES_ENCRYPT);

  return iv_raw.toHex() + out.toBase64();
}
QByteArray ClientSocket::aesDec(const QByteArray &in) {
  if (!aes_ready) {
    return in;
  }

  int num = 0;
  auto iv = in.first(32);
  auto aes_iv = QByteArray::fromHex(iv);

  auto real_in = in;
  real_in.remove(0, 32);
  auto inenc = QByteArray::fromBase64(real_in);
  QByteArray out;
  out.resize(inenc.length());
  unsigned char tempIv[16];
  strncpy((char *)tempIv, aes_iv.constData(), 16);
  AES_cfb128_encrypt((const unsigned char *)inenc.constData(),
                     (unsigned char *)out.data(), inenc.length(), &aes_key,
                     tempIv, &num, AES_DECRYPT);

  return out;
}
*/
