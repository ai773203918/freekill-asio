// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/router.h"
#include "network/client_socket.h"
#include "server/user/player.h"
#include "server/server.h"
#include "core/c-wrapper.h"

Router::Router(Player *player, ClientSocket *socket, RouterType type) {
  this->type = type;
  this->player = player;
  this->socket = nullptr;
  setSocket(socket);

  m_thread_id = std::this_thread::get_id();
}

Router::~Router() {
  abortRequest();
}

ClientSocket *Router::getSocket() const { return socket; }

void Router::setSocket(ClientSocket *socket) {
  if (this->socket != nullptr) {
    this->socket->set_message_got_callback([](Packet&){});
    this->socket->set_disconnected_callback([](){});
  }

  this->socket = nullptr;
  if (socket != nullptr) {
    socket->set_message_got_callback(
      std::bind(&Router::handlePacket, this, std::placeholders::_1));
    socket->set_disconnected_callback(
      std::bind(&Player::onDisconnected, player));
    this->socket = socket;
  }
}

void Router::removeSocket() {
  socket = nullptr;
}

void Router::set_reply_ready_callback(std::function<void()> callback) {
  reply_ready_callback = std::move(callback);
}

void Router::set_notification_got_callback(std::function<void(const Packet &)> callback) {
  notification_got_callback = std::move(callback);
}

void Router::request(int type, const std::string_view &command,
                     const std::string_view &cborData, int timeout, int64_t timestamp) {
  static int requestId = 0;
  requestId++;
  if (requestId > 10000000) requestId = 1;

  std::lock_guard<std::mutex> lock(replyMutex);
  expectedReplyId = requestId;
  replyTimeout = timeout;

  using namespace std::chrono;
  requestStartTime =
    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

  m_reply = "__notready";

  sendMessage(Cbor::encodeArray({
    requestId,
    type,
    command,
    cborData,
    timeout,
    (timestamp <= 0 ? requestStartTime : timestamp)
  }));
}

void Router::notify(int type, const std::string_view &command, const std::string_view &data) {
  if (!socket) return;
  auto buf = Cbor::encodeArray({
    -2,
    Router::TYPE_NOTIFICATION | Router::SRC_SERVER | Router::DEST_CLIENT,
    command,
    data,
  });
  sendMessage(buf);
}

// timeout永远是0
std::string Router::waitForReply(int timeout) {
  std::lock_guard<std::mutex> lock(replyMutex);
  return m_reply;
}

void Router::abortRequest() {
  std::lock_guard<std::mutex> lock(replyMutex);
  m_reply = "";
  // TODO wake up room?
}

void Router::handlePacket(const Packet &packet) {
  int requestId = packet.requestId;
  int type = packet.type;
  auto command = packet.command;
  auto cborData = packet.cborData;

  if (type & TYPE_NOTIFICATION) {
    notification_got_callback(packet);
  } else if (type & TYPE_REPLY) {
    using namespace std::chrono;
    std::lock_guard<std::mutex> lock(replyMutex);

    if (requestId != this->expectedReplyId)
      return;

    this->expectedReplyId = -1;

    auto now = duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();
    if (replyTimeout >= 0 &&
      replyTimeout * 1000 < now - requestStartTime)

      return;

    m_reply = cborData;
    // TODO: callback?

    reply_ready_callback();
  }
}

void Router::sendMessage(const std::string_view &msg) {
  if (!socket) return;
  // 根据本文 当发小包时应该可以认为线程安全
  // 具体需要人多点的时候测试
  // https://stackoverflow.com/questions/7362894/boostasiosocket-thread-safety
  if (msg.size() > 65535 &&
    std::this_thread::get_id() != m_thread_id) {

    // 将send任务交给主进程（如同Qt）并等待
    auto &s = Server::instance().context();
    auto p = std::promise<bool>();
    auto f = p.get_future();
    asio::post(s, [&](){
      socket->send({ msg.data(), msg.size() });
      p.set_value(true);
    });
    f.wait();

  } else {
    socket->send({ msg.data(), msg.size() });
  }
}
