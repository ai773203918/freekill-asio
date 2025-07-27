// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/router.h"
#include "network/client_socket.h"
#include "server/user/player.h"
#include "core/c-wrapper.h"

Router::Router(Player *player, ClientSocket *socket, RouterType type) {
  this->type = type;
  this->player = player;
  this->socket = nullptr;
  setSocket(socket);
  // expectedReplyId = -1;
  // replyTimeout = 0;
  // extraReplyReadySemaphore = nullptr;
}

Router::~Router() {
  //abortRequest();
}

ClientSocket *Router::getSocket() const { return socket; }

void Router::setSocket(ClientSocket *socket) {
  if (this->socket != nullptr) {
    // this->socket->disconnect(this);
    // disconnect(this->socket);
    // this->socket->deleteLater();
  }

  this->socket = nullptr;
  if (socket != nullptr) {
    // connect(this, &Router::messageReady, socket, &ClientSocket::send);
    socket->set_message_got_callback(
      std::bind(&Router::handlePacket, this, std::placeholders::_1));
    socket->set_disconnected_callback(
      std::bind(&Player::onDisconnected, player));
    // socket->setParent(this);
    this->socket = socket;
  }
}

void Router::removeSocket() {
  // socket->disconnect(this);
  socket = nullptr;
}

void Router::set_message_ready_callback(std::function<void(const std::vector<uint8_t>&)> callback) {
  message_ready_callback = std::move(callback);
}

void Router::set_reply_ready_callback(std::function<void()> callback) {
  reply_ready_callback = std::move(callback);
}

void Router::set_notification_got_callback(std::function<void(const Packet &)> callback) {
  notification_got_callback = std::move(callback);
}

/*
void Router::setReplyReadySemaphore(QSemaphore *semaphore) {
  extraReplyReadySemaphore = semaphore;
}

void Router::request(int type, const QByteArray &command, const QByteArray &cborData,
                     int timeout, qint64 timestamp) {
  // In case a request is called without a following waitForReply call
  if (replyReadySemaphore.available() > 0)
    replyReadySemaphore.acquire(replyReadySemaphore.available());

  static int requestId = 0;
  requestId++;
  if (requestId > 10000000) requestId = 1;

  replyMutex.lock();
  expectedReplyId = requestId;
  replyTimeout = timeout;
  requestStartTime = QDateTime::currentDateTime();
  m_reply = QStringLiteral("__notready");
  replyMutex.unlock();

  QCborArray body {
    requestId,
    type,
    command,
    cborData,
    timeout,
    (timestamp <= 0 ? requestStartTime.toMSecsSinceEpoch() : timestamp)
  };

  sendMessage(body.toCborValue().toCbor());
}

void Router::reply(int type, const QByteArray &command, const QByteArray &cborData) {
  QCborArray body {
    this->requestId,
    type,
    command,
    cborData,
  };

  sendMessage(body.toCborValue().toCbor());
}
*/

void Router::notify(int type, const std::string_view &command, const std::string_view &data) {
  if (!socket) return;
  auto buf = Cbor::encodeArray({
    -2,
    Router::TYPE_NOTIFICATION | Router::SRC_SERVER | Router::DEST_CLIENT,
    command,
    data,
  });
  socket->send({ buf.c_str(), buf.size() });
}

/*
int Router::getTimeout() const { return requestTimeout; }

// cancel last request from the sender
void Router::cancelRequest() {
  replyMutex.lock();
  expectedReplyId = -1;
  replyTimeout = 0;
  extraReplyReadySemaphore = nullptr;
  replyMutex.unlock();

  if (replyReadySemaphore.available() > 0)
    replyReadySemaphore.acquire(replyReadySemaphore.available());
}

QString Router::waitForReply(int timeout) {
  QString ret;
  replyReadySemaphore.tryAcquire(1, timeout * 1000);
  replyMutex.lock();
  ret = m_reply;
  replyMutex.unlock();
  return ret;
}

void Router::abortRequest() {
  replyMutex.lock();
  if (expectedReplyId != -1) {
    replyReadySemaphore.release();
    if (extraReplyReadySemaphore)
      extraReplyReadySemaphore->release();
    expectedReplyId = -1;
    extraReplyReadySemaphore = nullptr;
  }
  replyMutex.unlock();
}
*/

void Router::handlePacket(const Packet &packet) {
  int requestId = packet.requestId;
  int type = packet.type;
  auto command = packet.command;
  auto cborData = packet.cborData;

  if (type & TYPE_NOTIFICATION) {
    notification_got_callback(packet);
  } else if (type & TYPE_REPLY) {
    /*
    QMutexLocker locker(&replyMutex);

    if (requestId != this->expectedReplyId)
      return;

    this->expectedReplyId = -1;

    if (replyTimeout >= 0 &&
      replyTimeout < requestStartTime.secsTo(QDateTime::currentDateTime()))
      return;

    m_reply = cborData;
    // TODO: callback?
    replyReadySemaphore.release();
    if (extraReplyReadySemaphore) {
      extraReplyReadySemaphore->release();
      extraReplyReadySemaphore = nullptr;
    }

    locker.unlock();
    emit replyReady();
    */
  }
}

/*
void Router::sendMessage(const QByteArray &msg) {
  auto connType = qApp->thread() == QThread::currentThread()
    ? Qt::DirectConnection : Qt::BlockingQueuedConnection;
  QMetaObject::invokeMethod(qApp, [&]() { emit messageReady(msg); }, connType);
}
*/
