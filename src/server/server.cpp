// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/server.h"
#include "server/user/user_manager.h"
#include "network/server_socket.h"

static std::unique_ptr<Server> server_instance = nullptr;

std::unique_ptr<Server> &Server::instance() {
  if (!server_instance) {
    server_instance = std::unique_ptr<Server>(new Server);
  }
  return server_instance;
}

Server::Server() : m_socket { nullptr } {
  m_user_manager = std::make_unique<UserManager>();
  /*
  db = new Sqlite3;
  md5 = calcFileMD5();
  readConfig();

  auth = new AuthManager(this);

  nextRoomId = 1;
  m_lobby = new Lobby(this);

  // 启动心跳包线程
  auto heartbeatThread = QThread::create([=]() {
    while (true) {
      for (auto p : this->players.values()) {
        if (p->getState() == Player::Online) {
          p->alive = false;
          p->doNotify("Heartbeat", "");
        }
      }

      for (int i = 0; i < 30; i++) {
        if (!this->isListening) {
          return;
        }
        QThread::sleep(1);
      }

      for (auto p : this->players.values()) {
        if (p->getState() == Player::Online && !p->alive) {
          emit p->kicked();
        }
      }
    }
  });
  heartbeatThread->setParent(this);
  heartbeatThread->setObjectName("Heartbeat");
  heartbeatThread->start();
  */
}

Server::~Server() {
  /*
  isListening = false;
  // 虽然都是子对象 但析构顺序要抠一下
  for (auto p : findChildren<Player *>()) {
    delete p;
  }
  // 得先清理threads及其Rooms 因为其中某些析构函数要调用sql
  for (auto thr : findChildren<RoomThread *>()) {
    delete thr;
  }
  delete db;

  */
  server_instance = nullptr;
}

void Server::listen(asio::io_context &io_ctx, asio::ip::tcp::endpoint end) {
  m_socket = std::make_unique<ServerSocket>(io_ctx, end);

  // connect(m_socket, SIGNAL(new_connection), user_manager, SLOT(processNewConnection))
  m_socket->set_new_connection_callback(
    std::bind(&UserManager::processNewConnection, m_user_manager.get(), std::placeholders::_1));

  m_socket->listen();
}

std::unique_ptr<UserManager> &Server::user_manager() {
  return m_user_manager;
}

/*
void Server::createRoom(Player *owner, const QString &name, int capacity,
                        int timeout, const QByteArray &settings) {
  if (!checkBanWord(name)) {
    if (owner) {
      owner->doNotify("ErrorMsg", "unk error");
    }
    return;
  }
  Room *room;
  RoomThread *thread = nullptr;

  for (auto t : findChildren<RoomThread *>()) {
    if (!t->isFull() && !t->isOutdated()) {
      thread = t;
      break;
    }
  }

  if (!thread) {
    thread = new RoomThread(this);
  }

  room = new Room(thread);

  rooms.insert(room->getId(), room);
  room->setName(name);
  room->setCapacity(capacity);
  room->setTimeout(timeout);
  room->setSettings(settings);
  room->addPlayer(owner);
  room->setOwner(owner);
}

void Server::removeRoom(int id) {
  rooms.remove(id);
}

Room *Server::findRoom(int id) const { return rooms.value(id); }

Lobby *Server::lobby() const { return m_lobby; }

void Server::updateRoomList(Player *teller) {
  QCborArray arr;
  QCborArray avail_arr;
  for (Room *room : rooms) {
    QCborArray obj;
    auto settings = room->getSettingsObject();
    auto password = settings["password"].toString();
    auto count = room->getPlayers().count(); // playerNum
    auto cap = room->getCapacity();          // capacity

    obj << room->getId();        // roomId
    obj << room->getName();      // roomName
    obj << settings["gameMode"].toString(); // gameMode
    obj << count;
    obj << cap;
    obj << !password.isEmpty();
    obj << room->isOutdated();

    if (count == cap)
      arr << obj;
    else
      avail_arr << obj;
  }
  for (auto v : avail_arr) {
    arr.prepend(v);
  }
  teller->doNotify("UpdateRoomList", arr.toCborValue().toCbor());
}

void Server::updateOnlineInfo() {
  lobby()->doBroadcastNotify(lobby()->getPlayers(), "UpdatePlayerNum",
                             QCborArray{
                                 lobby()->getPlayers().length(),
                                 this->players.count(),
                             }.toCborValue().toCbor());
}

Sqlite3 *Server::getDatabase() { return db; }

void Server::broadcast(const QByteArray &command, const QByteArray &jsonData) {
  for (Player *p : players.values()) {
    p->doNotify(command, jsonData);
  }
}

void Server::sendEarlyPacket(ClientSocket *client, const QByteArray &type, const QByteArray &msg) {
  QCborArray body {
    -2,
    (Router::TYPE_NOTIFICATION | Router::SRC_SERVER | Router::DEST_CLIENT),
    type,
    msg,
  };
  client->send(body.toCborValue().toCbor());
}

#define SET_DEFAULT_CONFIG(k, v) do {\
  if (config.value(k).isUndefined()) { \
    config[k] = (v); \
  } } while (0)

void Server::readConfig() {
  QFile file("freekill.server.config.json");
  QByteArray json = QByteArrayLiteral("{}");
  if (file.open(QIODevice::ReadOnly)) {
    json = file.readAll();
  }
  config = QJsonDocument::fromJson(json).object();

  auto whitelist_json = getConfig("whitelist");
  if (whitelist_json.isArray()) {
    hasWhitelist = true;
    whitelist = whitelist_json.toArray().toVariantList();
  }

  // defaults
  SET_DEFAULT_CONFIG("description", "FreeKill Server");
  SET_DEFAULT_CONFIG("iconUrl", "default");
  SET_DEFAULT_CONFIG("capacity", 100);
  SET_DEFAULT_CONFIG("tempBanTime", 20);
  SET_DEFAULT_CONFIG("motd", "Welcome!");
  SET_DEFAULT_CONFIG("hiddenPacks", QJsonArray());
  SET_DEFAULT_CONFIG("enableBots", true);
  SET_DEFAULT_CONFIG("roomCountPerThread", 2000);
  SET_DEFAULT_CONFIG("maxPlayersPerDevice", 5);
}

QJsonValue Server::getConfig(const QString &key) { return config.value(key); }

bool Server::checkBanWord(const QString &str) {
  auto arr = getConfig("banwords").toArray();
  if (arr.isEmpty()) {
    return true;
  }
  for (auto v : arr) {
    auto s = v.toString().toUpper();
    if (str.toUpper().indexOf(s) != -1) {
      return false;
    }
  }
  return true;
}

void Server::temporarilyBan(int playerId) {
  auto player = findPlayer(playerId);
  if (!player) return;

  auto socket = player->getSocket();
  QString addr;
  if (!socket) {
    QString sql_find = QString("SELECT * FROM userinfo \
        WHERE id=%1;").arg(playerId);
    auto result = db->select(sql_find);
    if (result.isEmpty())
      return;

    auto obj = result[0];
    addr = obj["lastLoginIp"];
  } else {
    addr = socket->peerAddress();
  }
  temp_banlist.append(addr);

  auto time = getConfig("tempBanTime").toInt();
  QTimer::singleShot(time * 60000, this, [=]() {
      temp_banlist.removeOne(addr);
      });
  emit player->kicked();
}

void Server::beginTransaction() {
  transaction_mutex.lock();
  db->exec("BEGIN;");
}

void Server::endTransaction() {
  db->exec("COMMIT;");
  transaction_mutex.unlock();
}

const QString &Server::getMd5() const {
  return md5;
}

void Server::refreshMd5() {
  md5 = calcFileMD5();
  for (auto room : rooms) {
    if (room->isOutdated()) {
      if (!room->isStarted()) {
        for (auto p : room->getPlayers()) {
          p->doNotify("ErrorMsg", "room is outdated");
          p->kicked();
        }
      } else {
        room->doBroadcastNotify(room->getPlayers(), "GameLog", QCborMap {
          { "type", "#RoomOutdated" },
          { "toast", true },
        }.toCborValue().toCbor());
      }
    }
  }
  for (auto thread : findChildren<RoomThread *>()) {
    if (thread->isOutdated() && thread->findChildren<Room *>().isEmpty())
      thread->deleteLater();
  }
  for (auto p : lobby()->getPlayers()) {
    emit p->kicked();
  }
}

qint64 Server::getUptime() const {
  if (!uptime_counter.isValid()) return 0;
  return uptime_counter.elapsed();
}

bool Server::nameIsInWhiteList(const QString &name) const {
  if (!hasWhitelist) return true;
  return whitelist.length() > 0 && whitelist.contains(name);
}
*/
