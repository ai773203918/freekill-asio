// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/user/player.h"
#include "server/server.h"
#include "server/room/room_manager.h"
#include "server/room/roombase.h"
#include "network/client_socket.h"
#include "network/router.h"

#include <uuid/uuid.h>

static int nextConnId = 0;

Player::Player() {
  m_router = std::make_unique<Router>(this, nullptr, Router::TYPE_SERVER);

  // connect(router, &Router::notification_got, this, &Player::onNotificationGot);
  m_router->set_notification_got_callback(
    std::bind(&Player::onNotificationGot, this, std::placeholders::_1));
  // connect(router, &Router::replyReady, this, &Player::onReplyReady);

  roomId = 0;
  // connect(this, &Player::kicked, this, &Player::kick);
  // connect(this, &Player::stateChanged, this, &Player::onStateChanged);
  // connect(this, &Player::readyChanged, this, &Player::onReadyChanged);

  connId = nextConnId++;
  if (nextConnId >= 0x7FFFFF00) nextConnId = 0;

  alive = true;
  m_thinking = false;
}

Player::~Player() {}

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
}

bool Player::isReady() const { return ready; }

void Player::setReady(bool ready) {
  this->ready = ready;
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

RoomBase &Player::getRoom() const {
  auto &room_manager = Server::instance().room_manager();
  return *room_manager.findRoom(roomId);
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

/*
void Player::doRequest(const QByteArray &command, const QByteArray &jsonData,
                             int timeout, qint64 timestamp) {
  if (getState() != Player::Online)
    return;

  int type = Router::TYPE_REQUEST | Router::SRC_SERVER | Router::DEST_CLIENT;
  router->request(type, command, jsonData, timeout, timestamp);
}

void Player::abortRequest() { router->abortRequest(); }

std::string Player::waitForReply(int timeout) {
  std::string ret;
  if (getState() != Player::Online) {
#ifndef QT_DEBUG
    QThread::sleep(1);
#endif
    ret = std::stringLiteral("__cancel");
  } else {
    ret = router->waitForReply(timeout);
  }
  return ret;
}
*/

void Player::doNotify(const std::string_view &command, const std::string_view &data) {
  if (getState() != Player::Online)
    return;

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

void Player::set_state_changed_callback(std::function<void()> callback) {
  state_changed_callback = std::move(callback);
}

void Player::set_ready_changed_callback(std::function<void()> callback) {
  ready_changed_callback = std::move(callback);
}

void Player::set_kicked_callback(std::function<void()> callback) {
  kicked_callback = std::move(callback);
}

void Player::onNotificationGot(const Packet &packet) {
  if (packet.command == "Heartbeat") {
    alive = true;
    return;
  }

  auto &room_manager = Server::instance().room_manager();
  auto room = room_manager.findRoom(roomId);
  room->handlePacket(*this, packet);
}

void Player::onDisconnected() {
  spdlog::info("Player {} disconnected", id);
  // if (server->getPlayers().count() <= 10) {
  //     server->broadcast("ServerMessage", tr("%1 logged out").arg(getScreenName()).toUtf8());;
  // }

  /* TODO 这段逻辑转交给Room处理
  auto _room = getRoom();
  if (_room->isLobby()) {
    setState(Player::Robot); // 大厅！然而又不能设Offline
    deleteLater();
  } else {
    auto room = qobject_cast<Room *>(_room);
    if (room->isStarted()) {
      if (room->getObservers().contains(this)) {
        room->removeObserver(this);
        deleteLater();
        return;
      }
      if (thinking()) {
        auto thread = qobject_cast<RoomThread *>(room->parent());
        thread->wakeUp(room->getId(), "player_disconnect");
      }
      setState(Player::Offline);
      setSocket(nullptr);
      // TODO: add a robot
    } else {
      setState(Player::Robot); // 大厅！然而又不能设Offline
      // 这里有一个多线程问题，可能与Room::gameOver同时deleteLater导致出事
      // FIXME: 这种解法肯定不安全
      if (!room->insideGameOver)
        deleteLater();
    }
  }
  */
}

/*
 *
Player::~Player() {
  // 机器人直接被Room删除了
  if (getId() < 0) return;

  // 真人的话 需要先退出房间，再退出大厅
  room->removePlayer(this);
  if (room != nullptr) {
    room->removePlayer(this);
  }

  // 最后服务器删除他
  if (server->findPlayer(getId()) == this)
    server->removePlayer(getId());

  server->removePlayerByConnId(connId);
}

// 处理跑路玩家专用，就单纯把socket置为null
// 因为后面还会用到socket所以不删除
void Player::removeSocket() {
  socket->disconnect(this);
  socket = nullptr;
  router->removeSocket();
}

void Player::speak(const std::string &message) { ; }
void Player::prepareForRequest(const std::string &command,
                                     const std::string &data) {
  requestCommand = command;
  requestData = data;
}

void Player::kick() {
  setState(Player::Offline);
  if (socket != nullptr) {
    socket->disconnectFromHost();
  } else {
    // 还是得走一遍这个流程才行
    onDisconnected();
  }
  setSocket(nullptr);
}

void Player::reconnect(ClientSocket *client) {
  if (server->getPlayers().count() <= 10) {
    server->broadcast("ServerMessage", tr("%1 backed").arg(getScreenName()).toUtf8());
  }

  setState(Player::Online);
  setSocket(client);
  alive = true;
  // client->disconnect(this);

  if (room && !room->isLobby()) {
    server->setupPlayer(this, true);
    qobject_cast<Room *>(room)->pushRequest(std::string("%1,reconnect").arg(getId()));
  } else {
    // 懒得处理掉线玩家在大厅了！踢掉得了
    doNotify("ErrorMsg", "Unknown Error");
    emit kicked();
  }
}

void Player::startGameTimer() {
  gameTime = 0;
  gameTimer.start();
}

void Player::pauseGameTimer() {
  gameTime += gameTimer.elapsed() / 1000;
}

void Player::resumeGameTimer() {
  gameTimer.start();
}

int Player::getGameTime() {
  return gameTime + (getState() == Player::Online ? gameTimer.elapsed() / 1000 : 0);
}

void Player::onReplyReady() {
  setThinking(false);
  if (!room->isLobby()) {
    auto _room = qobject_cast<Room *>(room);
    auto thread = qobject_cast<RoomThread *>(_room->parent());
    thread->wakeUp(_room->getId(), "reply");
  }
}

void Player::onStateChanged() {
  auto _room = getRoom();
  if (!_room || _room->isLobby()) return;
  auto room = qobject_cast<Room *>(_room);
  if (room->hasObserver(this)) return;

  auto thread = qobject_cast<RoomThread *>(room->parent());
  if (thread) {
    emit thread->setPlayerState(connId, room->getId());
  }

  if (room->isAbandoned()) return;

  auto state = getState();
  room->doBroadcastNotify(room->getPlayers(), "NetStateChanged",
      QCborArray { getId(), getStateString() }.toCborValue().toCbor());

  if (state == Player::Online) {
    resumeGameTimer();
  } else {
    pauseGameTimer();
  }
}

void Player::onReadyChanged() {
  if (room && !room->isLobby()) {
    room->doBroadcastNotify(room->getPlayers(), "ReadyChanged",
                            QCborArray { getId(), isReady() }.toCborValue().toCbor());
  }
}
*/
