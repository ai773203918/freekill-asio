// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/room/room.h"
#include "server/room/lobby.h"
#include "server/room/room_manager.h"
#include "server/gamelogic/roomthread.h"
#include "network/client_socket.h"
#include "network/router.h"
#include "server/server.h"
#include "server/user/player.h"
#include "server/user/user_manager.h"
#include "core/c-wrapper.h"

Room::Room() {
  static int nextRoomId = 1;
  id = nextRoomId++;

  m_thread_id = 1000; // TODO

  // connect(this, &Room::abandoned, thread, &RoomThread::onRoomAbandoned);

  // m_abandoned = false;
  // gameStarted = false;
  // robot_id = -2; // -1 is reserved in UI logic
  // timeout = 15;
}

Room::~Room() {
  spdlog::debug("Room {} destructed", id);
  // 标记为过期 避免封人
  md5 = "";

  auto &um = Server::instance().user_manager();
  auto &rm = Server::instance().room_manager();

  m_session = nullptr;
  for (auto pConnId : players) {
    auto p = um.findPlayerByConnId(pConnId);
    if (!p) continue;

    if (p->getId() > 0) removePlayer(*p);
    else um.deletePlayer(*p);
  }
  for (auto pConnId : observers) {
    auto p = um.findPlayerByConnId(pConnId);
    if (p) removeObserver(*p);
  }

  auto thr = Server::instance().getThread(m_thread_id);
  if (thr) thr->decreaseRefCount();
  rm.removeRoom(getId());
  rm.lobby().updateOnlineInfo();
}

std::string &Room::getName() { return name; }

void Room::setName(const std::string_view &name) { this->name = name; }

size_t Room::getCapacity() const { return capacity; }

void Room::setCapacity(size_t capacity) { this->capacity = capacity; }

bool Room::isFull() const { return players.size() == capacity; }

const std::vector<int> &Room::getPlayers() const { return players; }
const std::vector<int> &Room::getObservers() const { return observers; }

const std::string_view Room::getSettings() const { return settings; }
const std::string_view Room::getGameMode() const { return gameMode; }
const std::string_view Room::getPassword() const { return password; }

void Room::setSettings(const std::string_view &settings) {
  // settings要失效了 先清空两个view
  gameMode = "";
  password = "";
  this->settings = settings;

  auto cbuf = (cbor_data)this->settings.data();
  auto len = this->settings.size();

  // 由于settings的内容实在无法预料到后面会变得多么复杂 这里直接分配内存了
  // 不抠那几百字节了

  struct cbor_load_result result;
  auto settings_map = cbor_load(cbuf, len, &result);
  if (result.error.code != CBOR_ERR_NONE || !cbor_isa_map(settings_map)) {
    cbor_decref(&settings_map);
    return;
  }

  auto sz = cbor_map_size(settings_map);
  auto pairs = cbor_map_handle(settings_map);
  int iter = 0;
  for (size_t i = 0; i < sz; i++) {
    auto pair = pairs[i];
    auto k = pair.key, v = pair.value;

    if (cbor_isa_string(k) && strncmp((const char *)cbor_string_handle(k), "gameMode", 8) == 0) {
      if (!cbor_isa_string(v)) continue;
      gameMode = std::string { (const char *)cbor_string_handle(v), cbor_string_length(v) };
      iter++;
    }

    if (cbor_isa_string(k) && strncmp((const char *)cbor_string_handle(k), "password", 8) == 0) {
      if (!cbor_isa_string(v)) continue;
      password = std::string { (const char *)cbor_string_handle(v), cbor_string_length(v) };
      iter++;
    }

    if (iter >= 2) break;
  }

  cbor_decref(&settings_map);
}

bool Room::isAbandoned() const {
  if (players.empty())
    return true;

  auto &um = Server::instance().user_manager();
  for (auto connId : players) {
    auto p = um.findPlayerByConnId(connId);
    if (p && p->getState() == Player::Online)
      return false;
  }
  return true;
}

Player *Room::getOwner() const {
  return Server::instance().user_manager().findPlayerByConnId(m_owner_conn_id);
}

void Room::setOwner(Player &owner) {
  // BOT不能当房主！
  if (owner.getId() < 0) return;
  m_owner_conn_id = owner.getConnId();
  doBroadcastNotify(players, "RoomOwner", Cbor::encodeArray( { owner.getId() } ));
}

void Room::addPlayer(Player &player) {
  auto pid = player.getId();
  if (isRejected(player)) {
    player.doNotify("ErrorMsg", "rejected your demand of joining room");
    return;
  }

  // 如果要加入的房间满员了，或者已经开战了，就不能再加入
  if (isFull() || gameStarted) {
    player.doNotify("ErrorMsg", "Room is full or already started!");
    return;
  }

  auto mode = gameMode;

  // 告诉房里所有玩家有新人进来了
  doBroadcastNotify(players, "AddPlayer", Cbor::encodeArray({
    pid,
    player.getScreenName().data(),
    player.getAvatar().data(),
    player.isReady(),
    player.getTotalGameTime(),
  }));

  players.push_back(player.getConnId());
  player.setRoom(*this);

  // 这集不用信号；这个信号是把玩家从大厅删除的
  // if (pid > 0)
  //   emit playerAdded(player);

  // Second, let the player enter room and add other players
  auto buf = Cbor::encodeArray({ capacity, timeout });
  buf.data()[0] += 1; // array header 2 -> 3
  player.doNotify("EnterRoom", buf + settings);

  auto &um = Server::instance().user_manager();
  for (auto connId : players) {
    if (connId == player.getConnId()) continue;
    auto p = um.findPlayerByConnId(connId);
    if (!p) continue; // FIXME: 应当是出大问题了
    player.doNotify("AddPlayer", Cbor::encodeArray({
      p->getId(),
      p->getScreenName(),
      p->getAvatar(),
      p->isReady(),
      p->getTotalGameTime(),
    }));

    player.doNotify("UpdateGameData", Cbor::encodeArray({
      p->getId(),
      // TODO 把傻逼gameData数组拿下
      p->getGameData()[0],
      p->getGameData()[1],
      p->getGameData()[2],
    }));
  }

  if (m_owner_conn_id == 0) {
    setOwner(player);
  }
  auto owner = um.findPlayerByConnId(m_owner_conn_id);
  if (owner)
    player.doNotify("RoomOwner", Cbor::encodeArray({ owner->getId() }));

  if (player.getLastGameMode() != mode) {
    player.setLastGameMode(std::string(mode));
    updatePlayerGameData(pid, mode);
  } else {
    doBroadcastNotify(players, "UpdateGameData", Cbor::encodeArray({
      pid,
      // TODO 把傻逼gameData数组拿下
      player.getGameData()[0],
      player.getGameData()[1],
      player.getGameData()[2],
    }));
  }
}

void Room::addRobot(Player &player) {
  if (player.getConnId() != m_owner_conn_id || isFull())
    return;

  auto &um = Server::instance().user_manager();
  auto &robot = um.createRobot();

  addPlayer(robot);
}

void Room::removePlayer(Player &player) {
  auto pid = player.getId();
  // 如果是旁观者的话，就清旁观者
  if (hasObserver(player)) {
    removeObserver(player);
    return;
  }

  auto &um = Server::instance().user_manager();
  if (!gameStarted) {
    // 游戏还没开始的话，直接删除这名玩家
    if (auto it = std::find(players.begin(), players.end(), player.getConnId()); it != players.end()) {
      player.setReady(false);
      players.erase(it);
    }
    // 这集必须手动加入到大厅
    // emit playerRemoved(player);

    doBroadcastNotify(players, "RemovePlayer", Cbor::encodeArray({ player.getId() }));
  } else {
    // 否则给跑路玩家召唤个AI代打

    // 首先拿到跑路玩家的socket，然后把玩家的状态设为逃跑，这样自动被机器人接管
    ClientSocket *socket = player.getRouter().getSocket();
    player.setState(Player::Run);
    player.getRouter().setSocket(nullptr);

    // 设完state后把房间叫起来
    if (player.thinking()) {
      auto thread = this->thread();
      if (thread) thread->wakeUp(id, "player_disconnect");
    }

    if (!player.isDied()) {
      runned_players.push_back(player.getId());
    }

    // 然后基于跑路玩家的socket，创建一个新Player对象用来通信
    auto runner = std::make_shared<Player>();
    runner->setState(Player::Online);
    runner->getRouter().setSocket(socket);
    runner->setScreenName(std::string(player.getScreenName()));
    runner->setAvatar(std::string(player.getAvatar()));
    runner->setId(player.getId());
    auto gamedata = player.getGameData();
    runner->setGameData(gamedata[0], gamedata[1], gamedata[2]);
    runner->addTotalGameTime(player.getTotalGameTime());

    // 最后向服务器玩家列表中增加这个人
    // 原先的跑路机器人会在游戏结束后自动销毁掉
    um.addPlayer(runner);

    Server::instance().room_manager().lobby().addPlayer(*runner);

    // FIX 控制bug
    u_char buf[10];
    size_t buflen = cbor_encode_uint(runner->getId(), buf, 10);
    runner->doNotify("ChangeSelf", { (char*)buf, buflen });

    // 如果走小道的人不是单机启动玩家 且房没过期 那么直接ban
    if (!isOutdated() && !player.isDied()) {
      Server::instance().temporarilyBan(runner->getId());
    }
  }

  if (isAbandoned()) {
    m_owner_conn_id = 0;
    checkAbandoned();
  } else if (player.getConnId() == m_owner_conn_id) {
    auto new_owner = um.findPlayerByConnId(players[0]);
    if (new_owner) setOwner(*new_owner);
  }
}

void Room::addObserver(Player &player) {
  // 首先只能旁观在运行的房间，因为旁观是由Lua处理的
  if (!gameStarted) {
    player.doNotify("ErrorMsg", "Can only observe running room.");
    return;
  }

  if (isRejected(player)) {
    player.doNotify("ErrorMsg", "rejected your demand of joining room");
    return;
  }

  // 向observers中追加player，并从大厅移除player，然后将player的room设为this
  observers.push_back(player.getConnId());
  player.setRoom(*this);

  auto thread = this->thread();
  thread->addObserver(player.getConnId(), id);
  pushRequest(fmt::format("{},observe", player.getId()));
}

void Room::removeObserver(Player &player) {
  if (auto it = std::find(observers.begin(), observers.end(), player.getId()); it != observers.end()) {
    observers.erase(it);
  }

  if (player.getState() == Player::Online) {
    player.doNotify("Setup", Cbor::encodeArray({
      player.getId(),
      player.getScreenName(),
      player.getAvatar(),
    }));
  }

  auto thread = this->thread();
  if (thread) thread->removeObserver(player.getConnId(), id);
  pushRequest(fmt::format("{},leave", player.getId()));
}

bool Room::hasObserver(Player &player) const {
  return std::find(observers.begin(), observers.end(), player.getId()) != observers.end();
}

int Room::getTimeout() const { return timeout; }

void Room::setTimeout(int timeout) { this->timeout = timeout; }

void Room::delay(int ms) {
  auto thread = this->thread();
  if (thread) thread->delay(id, ms);
}

bool Room::isOutdated() {
  bool ret = md5 != Server::instance().getMd5();
  if (ret) md5 = "";
  return ret;
}

bool Room::isStarted() const { return gameStarted; }

RoomThread *Room::thread() const {
  return Server::instance().getThread(m_thread_id);
}

void Room::setThread(RoomThread &t) {
  m_thread_id = t.id();
  md5 = t.getMd5();
  t.increaseRefCount();
}

void Room::checkAbandoned() {
  if (!isAbandoned()) return;
  if (getRefCount() > 0) {
    auto thr = thread();
    if (thr) thr->wakeUp(id, "abandon");
    return;
  }

  auto &rm = Server::instance().room_manager();
  rm.removeRoom(id);
}

static constexpr const char *findPWinRate = "SELECT win, lose, draw "
            "FROM pWinRate WHERE id = {} and mode = '{}' and role = '{}';";

static constexpr const char *updatePWinRate = ("UPDATE pWinRate "
            "SET win = {}, lose = {}, draw = {} "
            "WHERE id = {} and mode = '{}' and role = '{}';");

static constexpr const char *insertPWinRate = ("INSERT INTO pWinRate "
            "(id, mode, role, win, lose, draw) "
            "VALUES ({}, '{}', '{}', {}, {}, {});");

static constexpr const char *findGWinRate = ("SELECT win, lose, draw "
            "FROM gWinRate WHERE general = '{}' and mode = '{}' and role = '{}';");

static constexpr const char *updateGWinRate = ("UPDATE gWinRate "
            "SET win = {}, lose = {}, draw = {} "
            "WHERE general = '{}' and mode = '{}' and role = '{}';");

static constexpr const char *insertGWinRate = ("INSERT INTO gWinRate "
            "(general, mode, role, win, lose, draw) "
            "VALUES ('{}', '{}', '{}', {}, {}, {});");

static constexpr const char *findRunRate = ("SELECT run "
      "FROM runRate WHERE id = {} and mode = '{}';");

static constexpr const char *updateRunRate = ("UPDATE runRate "
            "SET run = {} WHERE id = {} and mode = '{}';");

static constexpr const char *insertRunRate = ("INSERT INTO runRate "
            "(id, mode, run) VALUES ({}, '{}', {});");

void Room::updatePlayerWinRate(int id, const std::string_view &mode, const std::string_view &role, int game_result) {
  if (!Sqlite3::checkString(mode))
    return;
  auto &db = Server::instance().database();

  int win = 0;
  int lose = 0;
  int draw = 0;
  int run = 0;

  switch (game_result) {
  case 1: win++; break;
  case 2: lose++; break;
  case 3: draw++; break;
  default: break;
  }

  auto result = db.select(fmt::format(findPWinRate, id, mode, role));

  if (result.empty()) {
    db.exec(fmt::format(insertPWinRate, id, mode, role, win, lose, draw));
  } else {
    auto obj = result[0];
    win += std::stoi(obj["win"]);
    lose += std::stoi(obj["lose"]);
    draw += std::stoi(obj["draw"]);
    db.exec(fmt::format(updatePWinRate, win, lose, draw, id, mode, role));
  }

  if (std::find(runned_players.begin(), runned_players.end(), id) != runned_players.end()) {
    addRunRate(id, mode);
  }

  auto &um = Server::instance().user_manager();
  auto player = um.findPlayer(id);
  if (player && std::find(players.begin(), players.end(), player->getConnId()) != players.end()) {
    player->setLastGameMode(std::string(mode));
    updatePlayerGameData(id, mode);
  }
}

void Room::updateGeneralWinRate(const std::string_view &general, const std::string_view &mode, const std::string_view &role, int game_result) {
  if (!Sqlite3::checkString(general))
    return;
  if (!Sqlite3::checkString(mode))
    return;
  auto &db = Server::instance().database();

  int win = 0;
  int lose = 0;
  int draw = 0;
  int run = 0;

  switch (game_result) {
  case 1: win++; break;
  case 2: lose++; break;
  case 3: draw++; break;
  default: break;
  }

  auto result = db.select(fmt::format(findGWinRate, general, mode, role));

  if (result.empty()) {
    db.exec(fmt::format(insertGWinRate, general, mode, role, win, lose, draw));
  } else {
    auto obj = result[0];
    win += std::stoi(obj["win"]);
    lose += std::stoi(obj["lose"]);
    draw += std::stoi(obj["draw"]);
    db.exec(fmt::format(updateGWinRate, win, lose, draw, general, mode, role));
  }
}

void Room::addRunRate(int id, const std::string_view &mode) {
  int run = 1;
  auto &db = Server::instance().database();
  auto result = db.select(fmt::format(findRunRate, id, mode));

  if (result.empty()) {
    db.exec(fmt::format(insertRunRate, id, mode, run));
  } else {
    auto obj = result[0];
    run += std::stoi(obj["run"]);
    db.exec(fmt::format(updateRunRate, run, id, mode));
  }
}

void Room::updatePlayerGameData(int id, const std::string_view &mode) {
  static constexpr const char *findModeRate =
    "SELECT win, total FROM pWinRateView WHERE id = {} and mode = '{}';";

  if (id < 0) return;

  auto &server = Server::instance();
  auto &um = server.user_manager();
  auto &db = server.database();

  auto player = um.findPlayer(id);
  if (!player) return;
  if (player->getState() == Player::Robot || player->getRoom().isLobby()) {
    return;
  }

  int total = 0;
  int win = 0;
  int run = 0;

  auto result = db.select(fmt::format(findRunRate, id, mode));

  if (!result.empty()) {
    run = stoi(result[0]["run"]);
  }

  result = db.select(fmt::format(findModeRate, id, mode));

  if (!result.empty()) {
    total = stoi(result[0]["total"]);
    win = stoi(result[0]["win"]);
  }

  auto room = dynamic_cast<Room *>(&player->getRoom());
  player->setGameData(total, win, run);
  room->doBroadcastNotify(room->getPlayers(), "UpdateGameData",
                          Cbor::encodeArray({ player->getId(), total, win, run }));
}

// 多线程非常麻烦 把GameOver交给主线程完成去
void Room::gameOver() {
  auto &s = Server::instance();
  if (std::this_thread::get_id() != s.mainThreadId()) {
    auto &ctx = Server::instance().context();

    auto p = std::promise<bool>();
    auto f = p.get_future();
    asio::post(ctx, [this, &p](){
      m_session = nullptr;
      p.set_value(true);
    });
    f.wait();
  } else {
    m_session = nullptr;
  }
}

Room::GameSession::GameSession(Room &r) : room { r } {
  room.gameStarted = true;

  auto thread = room.thread();
  if (thread) thread->pushRequest(fmt::format("-1,{},newroom", room.id));
}

Room::GameSession::~GameSession() {
  room.gameStarted = false;
  room.runned_players.clear();

  auto &server = Server::instance();
  auto &um = server.user_manager();
  const auto &mode = room.gameMode;
  std::vector<int> to_delete;

  // 首先只写数据库，这个过程不能向主线程提交申请(doNotify) 否则会死锁
  server.beginTransaction();
  for (auto pConnId : room.players) {
    auto p = um.findPlayerByConnId(pConnId);
    if (!p) continue;
    auto pid = p->getId();

    if (pid > 0) {
      // TODO 游玩时长统计
      // int time = p->getGameTime();

      // // 将游戏时间更新到数据库中
      // auto info_update = QString("UPDATE usergameinfo SET totalGameTime = "
      // "IIF(totalGameTime IS NULL, {}, totalGameTime + {}) WHERE id = {};").arg(pid).arg(time);
      // server->database()->exec(info_update);
    }

    if (p->getState() == Player::Offline) {
      room.addRunRate(pid, mode);
    }
  }
  server.endTransaction();

  for (auto pConnId : room.players) {
    auto p = um.findPlayerByConnId(pConnId);
    if (!p) continue;

    /* 计时相关 再说
    if (pid > 0) {
      int time = p->getGameTime();
      auto bytes = QCborArray { pid, time }.toCborValue().toCbor();
      doBroadcastNotify(getOtherPlayers(p), "AddTotalGameTime", bytes);

      // 考虑到阵亡已离开啥的，时间得给真实玩家增加
      auto realPlayer = server->findPlayer(pid);
      if (realPlayer) {
        realPlayer->addTotalGameTime(time);
        realPlayer->doNotify("AddTotalGameTime", bytes);
      }
    }
    */

    if (p->getState() != Player::Online) {
      if (p->getState() == Player::Offline) {
        if (!room.isOutdated()) {
          server.temporarilyBan(p->getId());
        } else {
          p->emitKicked();
        }
      }
      to_delete.push_back(pConnId);
      um.deletePlayer(*p);
    }
  }

  room.players.erase(std::remove_if(room.players.begin(), room.players.end(), [&](int x) {
    return std::find(to_delete.begin(),to_delete.end(), x) != to_delete.end();
  }), room.players.end());
}

void Room::manuallyStart() {
  if (isFull() && !gameStarted) {
    spdlog::info("[GameStart] Room {} started", getId());
    /* TODO 多开警告 我感觉单独拿个函数吧
    QMap<QString, QStringList> uuidList, ipList;
    for (auto p : players) {
      p->setReady(false);
      p->setDied(false);
      p->startGameTimer();

      if (p->getId() < 0) continue;
      auto uuid = p->getUuid();
      auto ip = p->getPeerAddress();
      auto pname = p->getScreenName();
      if (!uuid.isEmpty()) {
        uuidList[uuid].append(pname);
      }
      if (!ip.isEmpty()) {
        ipList[ip].append(pname);
      }
    }

    for (auto i = ipList.cbegin(); i != ipList.cend(); i++) {
      if (i.value().length() <= 1) continue;
      auto warn = QString("*WARN* Same IP address: [{}]").arg(i.value().join(", "));
      auto warnUtf8 = warn.toUtf8();
      doBroadcastNotify(getPlayers(), "ServerMessage", warnUtf8);
      qInfo("%s", warnUtf8.constData());
    }

    for (auto i = uuidList.cbegin(); i != uuidList.cend(); i++) {
      if (i.value().length() <= 1) continue;
      auto warn = QString("*WARN* Same device id: [{}]").arg(i.value().join(", "));
      auto warnUtf8 = warn.toUtf8();
      doBroadcastNotify(getPlayers(), "ServerMessage", warnUtf8);
      qInfo("%s", warnUtf8.constData());
    }
    */

    m_session = std::make_unique<Room::GameSession>(*this);
  }
}

void Room::pushRequest(const std::string &req) {
  auto thread = this->thread();
  if (thread) thread->pushRequest(fmt::format("{},{}", id, req));
}

void Room::addRejectId(int id) {
  rejected_players.push_back(id);
}

void Room::removeRejectId(int id) {
  if (auto it = std::find(rejected_players.begin(), rejected_players.end(), id);
    it != rejected_players.end()) {
    rejected_players.erase(it);
  }
}

bool Room::isRejected(Player &player) const {
  return std::find(rejected_players.begin(), rejected_players.end(), player.getId()) != rejected_players.end();
}

void Room::setPlayerReady(Player &p, bool ready) {
  p.setReady(ready);
  doBroadcastNotify(players, "ReadyChanged", Cbor::encodeArray({ p.getId(), ready }));
}

// ------------------------------------------------
void Room::quitRoom(Player &player, const Packet &) {
  removePlayer(player);
  auto &rm = Server::instance().room_manager();
  rm.lobby().addPlayer(player);

  if (isOutdated()) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayer(player.getId());
    if (p) p->emitKicked();
  }
}

void Room::addRobotRequest(Player &player, const Packet &) {
  if (Server::instance().config().enableBots)
    addRobot(player);
}

void Room::kickPlayer(Player &player, const Packet &pkt) {
  auto data = pkt.cborData;
  int i = 0;
  auto result = cbor_stream_decode((cbor_data)data.data(), data.size(), &Cbor::intCallbacks, &i);
  if (result.read == 0 || i == 0) return;

  auto &um = Server::instance().user_manager();
  auto &rm = Server::instance().room_manager();
  auto p = um.findPlayer(i);
  if (p && p->getRoom().getId() == id &&  !isStarted()) {
    removePlayer(*p);
    rm.lobby().addPlayer(*p);

    addRejectId(i);

    using namespace std::chrono_literals;
    auto timer = std::make_shared<asio::steady_timer>(Server::instance().context(), 30000ms);
    timer->async_wait([this, i, timer](const asio::error_code &ec) {
      if (!ec) {
        removeRejectId(i);
      } else {
        spdlog::error(ec.message());
      }
    });
  }
}

void Room::ready(Player &player, const Packet &) {
  setPlayerReady(player, !player.isReady());
}

void Room::startGame(Player &player, const Packet &) {
  if (isOutdated()) {
    auto &um = Server::instance().user_manager();
    for (auto pid : getPlayers()) {
      auto p = um.findPlayerByConnId(pid);
      if (!p) continue;
      p->doNotify("ErrorMsg", "room is outdated");
      p->emitKicked();
    }
  } else {
    manuallyStart();
  }
}

typedef void (Room::*room_cb)(Player &, const Packet &);

void Room::handlePacket(Player &sender, const Packet &packet) {
  static const std::unordered_map<std::string_view, room_cb> room_actions = {
    {"QuitRoom", &Room::quitRoom},
    {"AddRobot", &Room::addRobotRequest},
    {"KickPlayer", &Room::kickPlayer},
    {"Ready", &Room::ready},
    {"StartGame", &Room::startGame},
    {"Chat", &Room::chat},
  };

  if (packet.command == "PushRequest") {
    pushRequest(fmt::format("{},{}", sender.getId(), packet.cborData));
    return;
  }

  auto iter = room_actions.find(packet.command);
  if (iter != room_actions.end()) {
    auto func = iter->second;
    (this->*func)(sender, packet);
  }
}

// Lua用：request之前设置计时器防止等到死。
void Room::setRequestTimer(int ms) {
  auto thread = this->thread();
  if (!thread) return;
  auto &ctx = thread->context();

  request_timer = std::make_unique<asio::steady_timer>(
    ctx, std::chrono::milliseconds(ms));

  request_timer->async_wait([&](const asio::error_code& ec){
    if (!ec) {
      thread->wakeUp(id, "request_timer");
    } else {
      // 我们本来就会调cancel()并销毁requestTimer，所以aborted的情况很多很多
      if (ec != asio::error::operation_aborted) {
        spdlog::error("error in request timer of room {}: {}", id, ec.message());
      }
    }
  });
}

// Lua用：当request完成后手动销毁计时器。
void Room::destroyRequestTimer() {
  if (!request_timer) return;
  request_timer->cancel();
  request_timer = nullptr;
}

int Room::getRefCount() {
  std::lock_guard<std::mutex> locker(lua_ref_mutex);
  return lua_ref_count;
}

void Room::increaseRefCount() {
  std::lock_guard<std::mutex> locker(lua_ref_mutex);
  lua_ref_count++;
}

void Room::decreaseRefCount() {
  {
    std::lock_guard<std::mutex> locker(lua_ref_mutex);
    lua_ref_count--;
    if (lua_ref_count > 0) return;
  }
  checkAbandoned();
}
