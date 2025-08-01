// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/user/player.h"
#include "server/user/user_manager.h"
#include "server/server.h"
#include "server/gamelogic/roomthread.h"
#include "server/room/room_manager.h"
#include "server/room/roombase.h"
#include "server/room/room.h"
#include "server/room/lobby.h"
#include "network/client_socket.h"
#include "network/router.h"

#include "core/c-wrapper.h"
#include "core/util.h"

using namespace std::chrono;

static int nextConnId = 1000;

Player::Player() {
  m_router = std::make_unique<Router>(this, nullptr, Router::TYPE_SERVER);

  m_router->set_notification_got_callback(
    std::bind(&Player::onNotificationGot, this, std::placeholders::_1));
  m_router->set_reply_ready_callback(
    std::bind(&Player::onReplyReady, this));

  roomId = 0;

  connId = nextConnId++;
  if (nextConnId >= 0x7FFFFF00) nextConnId = 1000;

  alive = true;
  m_thinking = false;

  gameTime = 0;
  auto now = system_clock::now();
  gameTimerStartTimestamp = duration_cast<seconds>(now.time_since_epoch()).count();
}

Player::~Player() {
  // spdlog::debug("[MEMORY] Player {} (connId={} state={}) destructed", id, connId, getStateString());
}

int Player::getId() const { return id; }

void Player::setId(int id) { this->id = id; }

std::string_view Player::getScreenName() const { return screenName; }

void Player::setScreenName(const std::string &name) {
  this->screenName = name;
}

std::string_view Player::getAvatar() const { return avatar; }

void Player::setAvatar(const std::string &avatar) {
  this->avatar = avatar;
}

int Player::getTotalGameTime() const { return totalGameTime; }

void Player::addTotalGameTime(int toAdd) {
  totalGameTime += toAdd;
}

Player::State Player::getState() const { return state; }

std::string_view Player::getStateString() const {
  switch (state) {
  case Online:
    return "online";
  case Trust:
    return "trust";
  case Run:
    return "run";
  case Leave:
    return "leave";
  case Robot:
    return "robot";
  case Offline:
    return "offline";
  default:
    return "invalid";
  }
}

void Player::setState(Player::State state) {
  this->state = state;

  // QT祖宗之法不可变
  onStateChanged();
}

bool Player::isReady() const { return ready; }

void Player::setReady(bool ready) {
  this->ready = ready;
  onReadyChanged();
}

std::vector<int> Player::getGameData() {
  return { totalGames, winCount, runCount };
}

void Player::setGameData(int total, int win, int run) {
  totalGames = total;
  winCount = win;
  runCount = run;
}

std::string_view Player::getLastGameMode() const {
  return lastGameMode;
}

void Player::setLastGameMode(const std::string &mode) {
  lastGameMode = mode;
}

bool Player::isDied() const {
  return died;
}

void Player::setDied(bool died) {
  this->died = died;
}

int Player::getConnId() const { return connId; }

std::shared_ptr<RoomBase> Player::getRoom() const {
  auto &room_manager = Server::instance().room_manager();
  if (roomId == 0) {
    return room_manager.lobby();
  }
  return room_manager.findRoom(roomId);
}

void Player::setRoom(RoomBase &room) {
  roomId = room.getId();
}

Router &Player::router() const {
  return *m_router;
}

// std::string_view Player::getPeerAddress() const {
//   auto p = server->findPlayer(getId());
//   if (!p || p->getState() != Player::Online)
//     return "";
//   return p->getSocket()->peerAddress();
// }

std::string_view Player::getUuid() const {
  return uuid_str;
}

void Player::setUuid(const std::string &uuid) {
  uuid_str = uuid;
}

void Player::doRequest(const std::string_view &command,
                       const std::string_view &jsonData, int timeout, int64_t timestamp) {
  if (getState() != Player::Online)
    return;

  int type = Router::TYPE_REQUEST | Router::SRC_SERVER | Router::DEST_CLIENT;
  m_router->request(type, command, jsonData, timeout, timestamp);
}

std::string Player::waitForReply(int timeout) {
  std::string ret;
  if (getState() != Player::Online) {
    ret = "__cancel";
  } else {
    ret = m_router->waitForReply(timeout);
  }
  return ret;
}

void Player::doNotify(const std::string_view &command, const std::string_view &data) {
  if (getState() != Player::Online)
    return;

  // spdlog::debug("[TX](id={} connId={} state={} Room={}): {} {}", id, connId, getStateString(), roomId, command, toHex(data));
  int type =
      Router::TYPE_NOTIFICATION | Router::SRC_SERVER | Router::DEST_CLIENT;

  // 包体至少得传点东西，传个null吧
  m_router->notify(type, command, data == "" ? "\xF6" : data);
}

bool Player::thinking() {
  std::lock_guard<std::mutex> locker { m_thinking_mutex };
  return m_thinking;
}

void Player::setThinking(bool t) {
  std::lock_guard<std::mutex> locker { m_thinking_mutex };
  m_thinking = t;
}

void Player::onNotificationGot(const Packet &packet) {
  // spdlog::debug("[RX](id={} connId={} state={} Room={}): {} {}", id, connId, getStateString(), roomId, packet.command, toHex(packet.cborData));
  if (packet.command == "Heartbeat") {
    alive = true;
    return;
  }

  auto room = getRoom();
  if (room) room->handlePacket(*this, packet);
}

void Player::onDisconnected() {
  spdlog::info("Player {} disconnected{}", id,
               m_router->getSocket() != nullptr ? "" : " (pseudo)");

  auto &server = Server::instance();
  if (server.user_manager().getPlayers().size() <= 10) {
    server.broadcast("ServerMessage", fmt::format("{} logged out", screenName));
  }

  auto room_ = getRoom();
  auto &um = Server::instance().user_manager();
  if (!room_) {
    um.deletePlayer(*this);
  } else if (room_->isLobby()) {
    room_->removePlayer(*this);
    um.deletePlayer(*this);
  } else {
    auto room = dynamic_pointer_cast<Room>(room_);
    if (room->isStarted()) {
      if (room->hasObserver(*this)) {
        room->removeObserver(*this);
        um.deletePlayer(*this);
        return;
      }
      if (thinking()) {
        auto thread = room->thread();
        thread->wakeUp(room->getId(), "player_disconnect");
      }
      setState(Player::Offline);
      m_router->setSocket(nullptr);
      // TODO: add a robot
    } else {
      room->removePlayer(*this);
      um.deletePlayer(*this);
    }
  }
}

Router &Player::getRouter() { return *m_router; }

void Player::kick() {
  setState(Player::Offline);
  auto id = getId();
  if (m_router->getSocket() != nullptr) {
    m_router->getSocket()->disconnectFromHost();
  } else {
    // 还是得走一遍这个流程才行
    onDisconnected();
  }

  // 走到这里可能被析构了 重新找this
  auto p = Server::instance().user_manager().findPlayer(id);
  if (p) p->getRouter().setSocket(nullptr);
}

void Player::emitKicked() {
  auto &s = Server::instance();
  if (std::this_thread::get_id() != s.mainThreadId()) {
    auto &ctx = Server::instance().context();

    auto p = std::promise<bool>();
    auto f = p.get_future();
    asio::post(ctx, [this, &p](){
      kick();
      p.set_value(true);
    });
    f.wait();
  } else {
    kick();
  }
}

void Player::reconnect(ClientSocket *client) {
  auto &server = Server::instance();
  if (server.user_manager().getPlayers().size() <= 10) {
    server.broadcast("ServerMessage", fmt::format("{} backed", screenName));
  }

  setState(Player::Online);
  m_router->setSocket(client);
  alive = true;

  auto room_ = getRoom();
  if (room_ && !room_->isLobby()) {
    Server::instance().user_manager().setupPlayer(*this, true);
    auto room = dynamic_pointer_cast<Room>(room_);
    room->pushRequest(fmt::format("{},reconnect", id));
  } else {
    // 懒得处理掉线玩家在大厅了！踢掉得了
    doNotify("ErrorMsg", "Unknown Error");
    emitKicked();
  }
}

void Player::startGameTimer() {
  gameTime = 0;
  auto now = system_clock::now();
  gameTimerStartTimestamp = duration_cast<seconds>(now.time_since_epoch()).count();
}

void Player::pauseGameTimer() {
  auto now = system_clock::now();
  auto timestamp = duration_cast<seconds>(now.time_since_epoch()).count();
  gameTime += (timestamp - gameTimerStartTimestamp);
}

void Player::resumeGameTimer() {
  auto now = system_clock::now();
  gameTimerStartTimestamp = duration_cast<seconds>(now.time_since_epoch()).count();
}

int Player::getGameTime() {
  auto now = system_clock::now();
  auto timestamp = duration_cast<seconds>(now.time_since_epoch()).count();
  return gameTime + (getState() == Player::Online ? (timestamp - gameTimerStartTimestamp) : 0);
}

void Player::onReplyReady() {
  setThinking(false);
  auto room_ = getRoom();
  if (room_ && !room_->isLobby()) {
    auto room = dynamic_pointer_cast<Room>(room_);
    auto thread = room->thread();
    thread->wakeUp(room->getId(), "reply");
  }
}

void Player::onStateChanged() {
  auto _room = getRoom();
  if (!_room || _room->isLobby()) return;
  auto room = dynamic_pointer_cast<Room>(_room);
  if (room->hasObserver(*this)) return;

  auto thread = room->thread();
  thread->setPlayerState(connId, room->getId());

  if (room->isAbandoned()) return;

  room->doBroadcastNotify(room->getPlayers(), "NetStateChanged",
                          Cbor::encodeArray({ id, getStateString() }));

  auto state = getState();
  if (state == Player::Online) {
    resumeGameTimer();
  } else {
    pauseGameTimer();
  }
}

void Player::onReadyChanged() {
  auto _room = getRoom();
  if (!_room || _room->isLobby()) return;
  auto room = dynamic_pointer_cast<Room>(_room);
  room->doBroadcastNotify(room->getPlayers(), "ReadyChanged",
                          Cbor::encodeArray({ id, ready }));
}
