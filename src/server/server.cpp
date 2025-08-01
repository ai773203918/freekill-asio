// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/server.h"
#include "server/room/room_manager.h"
#include "server/room/room.h"
#include "server/room/lobby.h"
#include "server/user/user_manager.h"
#include "server/user/auth.h"
#include "server/user/player.h"
#include "network/server_socket.h"
#include "network/client_socket.h"
#include "network/router.h"
#include "server/gamelogic/roomthread.h"
#include "server/rpc-lua/rpc-lua.h"

#include "server/cli/shell.h"

#include "core/c-wrapper.h"
#include "core/util.h"

#include <cjson/cJSON.h>

static std::unique_ptr<Server> server_instance = nullptr;

Server &Server::instance() {
  if (!server_instance) {
    server_instance = std::unique_ptr<Server>(new Server);
  }
  return *server_instance;
}

Server::Server() : m_socket { nullptr } {
  m_user_manager = std::make_unique<UserManager>();
  m_room_manager = std::make_unique<RoomManager>();

  main_thread_id = std::this_thread::get_id();

  db = std::make_unique<Sqlite3>();

  reloadConfig();

  md5 = calcFileMD5();

  using namespace std::chrono;
  start_timestamp =
    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

  /*
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
  // server_instance = nullptr;
}

void Server::listen(asio::io_context &io_ctx, asio::ip::tcp::endpoint end, asio::ip::udp::endpoint uend) {
  main_io_ctx = &io_ctx;
  m_socket = std::make_unique<ServerSocket>(io_ctx, end, uend);

  m_shell = std::make_unique<Shell>();
  m_shell->start();

  // connect(m_socket, SIGNAL(new_connection), user_manager, SLOT(processNewConnection))
  m_socket->set_new_connection_callback(
    std::bind(&UserManager::processNewConnection, m_user_manager.get(), std::placeholders::_1));

  m_socket->listen();
  m_socket->listen_udp();
}

void Server::stop() {
  main_io_ctx->stop();
}

void Server::destroy() {
  server_instance = nullptr;
}

asio::io_context &Server::context() {
  return *main_io_ctx;
}

UserManager &Server::user_manager() {
  return *m_user_manager;
}

RoomManager &Server::room_manager() {
  return *m_room_manager;
}

Sqlite3 &Server::database() {
  return *db;
}

Shell &Server::shell() {
  return *m_shell;
}

void Server::sendEarlyPacket(ClientSocket &client, const std::string_view &type, const std::string_view &msg) {
  auto buf = Cbor::encodeArray({
    -2,
    Router::TYPE_NOTIFICATION | Router::SRC_SERVER | Router::DEST_CLIENT,
    type,
    msg,
  });
  client.send({ buf.c_str(), buf.size() });
}

RoomThread &Server::createThread() {
  auto thr = std::make_unique<RoomThread>(*main_io_ctx);
  auto id = thr->id();
  m_threads[id] = std::move(thr);
  return *m_threads[id];
}

void Server::removeThread(int threadId) {
  auto it = m_threads.find(threadId);
  if (it != m_threads.end()) {
    m_threads.erase(threadId);
  }
}

RoomThread *Server::getThread(int threadId) {
  auto it = m_threads.find(threadId);
  if (it != m_threads.end()) {
    return it->second.get();
  }
  return nullptr;
}

RoomThread &Server::getAvailableThread() {
  for (auto &it : m_threads) {
    // TODO 判满
    auto &thr = it.second;
    return *thr;
  }

  return createThread();
}

const std::unordered_map<int, std::unique_ptr<RoomThread>> &
  Server::getThreads() const
{
  return m_threads;
}

std::thread::id Server::mainThreadId() const {
  return main_thread_id;
}

void Server::broadcast(const std::string_view &command, const std::string_view &jsonData) {
  for (auto &[_, p] : user_manager().getPlayers()) {
    p->doNotify(command, jsonData);
  }
}

void ServerConfig::loadConf(const char* jsonStr) {
  cJSON* root = cJSON_Parse(jsonStr);
  if (!root) {
    spdlog::error("JSON parse error: {}", cJSON_GetErrorPtr());
    return;
  }

  cJSON* item = nullptr;

  if ((item = cJSON_GetObjectItem(root, "banWords")) && cJSON_IsArray(item)) {
    int size = cJSON_GetArraySize(item);
    banWords.clear();
    for (int i = 0; i < size; ++i) {
      cJSON* word = cJSON_GetArrayItem(item, i);
      if (cJSON_IsString(word) && word->valuestring) {
        banWords.push_back(word->valuestring);
      }
    }
  }

  if ((item = cJSON_GetObjectItem(root, "description")) && cJSON_IsString(item) && item->valuestring) {
    description = item->valuestring;
  }

  if ((item = cJSON_GetObjectItem(root, "iconUrl")) && cJSON_IsString(item) && item->valuestring) {
    iconUrl = item->valuestring;
  }

  if ((item = cJSON_GetObjectItem(root, "capacity")) && cJSON_IsNumber(item)) {
    capacity = static_cast<int>(item->valuedouble);
  }

  if ((item = cJSON_GetObjectItem(root, "tempBanTime")) && cJSON_IsNumber(item)) {
    tempBanTime = static_cast<int>(item->valuedouble);
  }

  if ((item = cJSON_GetObjectItem(root, "motd")) && cJSON_IsString(item) && item->valuestring) {
    motd = item->valuestring;
  }

  if ((item = cJSON_GetObjectItem(root, "hiddenPacks")) && cJSON_IsArray(item)) {
    int size = cJSON_GetArraySize(item);
    hiddenPacks.clear();
    for (int i = 0; i < size; ++i) {
      cJSON* pack = cJSON_GetArrayItem(item, i);
      if (cJSON_IsString(pack) && pack->valuestring) {
        hiddenPacks.push_back(pack->valuestring);
      }
    }
  }

  if ((item = cJSON_GetObjectItem(root, "enableBots")) && cJSON_IsBool(item)) {
    enableBots = cJSON_IsTrue(item);
  }

  if ((item = cJSON_GetObjectItem(root, "roomCountPerThread")) && cJSON_IsNumber(item)) {
    roomCountPerThread = static_cast<int>(item->valuedouble);
  }

  if ((item = cJSON_GetObjectItem(root, "maxPlayersPerDevice")) && cJSON_IsNumber(item)) {
    maxPlayersPerDevice = static_cast<int>(item->valuedouble);
  }

  cJSON_Delete(root);
}

void Server::reloadConfig() {
  std::string jsonStr = "{}";

  std::ifstream file("freekill.server.config.json", std::ios::binary);
  if (file.is_open()) {
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    jsonStr.resize(size);
    file.seekg(0, std::ios::beg);
    file.read(&jsonStr[0], size);
    file.close();
  }

  m_config = std::make_unique<ServerConfig>();
  m_config->loadConf(jsonStr.c_str());
}

const ServerConfig &Server::config() const { return *m_config; }

bool Server::checkBanWord(const std::string_view &str) {
  auto arr = m_config->banWords;
  for (auto &s : arr) {
    if (str.find(s) != -1) {
      return false;
    }
  }
  return true;
}

void Server::temporarilyBan(int playerId) {
  auto player = m_user_manager->findPlayer(playerId);
  if (!player) return;

  auto socket = player->getRouter().getSocket();
  std::string addr;
  if (!socket) {
    static constexpr const char *sql_find =
      "SELECT lastLoginIp FROM userinfo WHERE id={};";
    auto result = db->select(fmt::format(sql_find, playerId));
    if (result.empty())
      return;

    auto obj = result[0];
    addr = obj["lastLoginIp"];
  } else {
    addr = socket->peerAddress();
  }
  temp_banlist.push_back(addr);

  auto time = m_config->tempBanTime;
  using namespace std::chrono;
  auto timer = std::make_shared<asio::steady_timer>(*main_io_ctx, seconds(time * 60));
  timer->async_wait([this, addr, timer](const asio::error_code& ec){
    if (!ec) {
      auto it = std::find(temp_banlist.begin(), temp_banlist.end(), addr);
      if (it != temp_banlist.end())
        temp_banlist.erase(it);
    } else {
      spdlog::error("error in tempBan timer: {}", ec.message());
    }
  });
  player->emitKicked();
}

void Server::beginTransaction() {
  transaction_mutex.lock();
  db->exec("BEGIN;");
}

void Server::endTransaction() {
  db->exec("COMMIT;");
  transaction_mutex.unlock();
}

const std::string &Server::getMd5() const {
  return md5;
}

void Server::refreshMd5() {
  md5 = calcFileMD5();

  auto &rm = room_manager();
  for (auto &[_, room] : rm.getRooms()) {
    if (room->isOutdated()) {
      if (!room->isStarted()) {
        for (auto pConnId : room->getPlayers()) {
          auto p = m_user_manager->findPlayerByConnId(pConnId);
          if (p) p->emitKicked();
        }
      } else {
        // const char * 会给末尾加0 手造二进制数据的话必须考虑
        using namespace std::string_view_literals;
        static constexpr const auto log =
          "\xA2"                      // map(2)
          "\x44" "type"               // key(0) : bytes(4)
          "\x4D" "#RoomOutdated"      // value(0) : bytes(13)
          "\x45" "toast"              // key(1) : bytes(5)
          "\xF5"sv;                   // value(1): true
        room->doBroadcastNotify(room->getPlayers(), "GameLog", log);
      }
    }
  }
  for (auto &[_, thread] : m_threads) {
    // if (thread->isOutdated() && thread->findChildren<Room *>().isEmpty())
    //   thread->deleteLater();
  }
  for (auto &[pConnId, _] : rm.lobby().getPlayers()) {
    auto p = m_user_manager->findPlayerByConnId(pConnId);
    if (p) p->emitKicked();
  }
}

int64_t Server::getUptime() const {
  using namespace std::chrono;
  auto now =
    duration_cast<milliseconds>(system_clock::now().time_since_epoch()).count();

  return now - start_timestamp;
}

bool Server::nameIsInWhiteList(const std::string_view &name) const {
  // if (!hasWhitelist) return true;
  // return whitelist.length() > 0 && whitelist.contains(name);
  return true;
}
