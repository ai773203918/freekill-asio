// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class ServerSocket;
class ClientSocket;

class UserManager;
class RoomManager;
class RoomThread;

class Shell;
class Sqlite3;

using asio::ip::tcp;

struct ServerConfig {
  std::vector<std::string> banWords;
  std::string description = "FreeKill Server (non-Qt)";
  std::string iconUrl = "default";
  int capacity = 100;
  int tempBanTime = 0;
  std::string motd = "Welcome!";
  std::vector<std::string> hiddenPacks;
  bool enableBots = true;
  bool enableWhitelist = false;
  int roomCountPerThread = 2000;
  int maxPlayersPerDevice = 1000;

  void loadConf(const char *json);

  ServerConfig() = default;
  ServerConfig(ServerConfig &) = delete;
  ServerConfig(ServerConfig &&) = delete;
};

class Server {
public:
  static Server &instance();

  Server(Server &) = delete;
  Server(Server &&) = delete;
  ~Server();

  void listen(asio::io_context &io_ctx, tcp::endpoint end, asio::ip::udp::endpoint uend);
  void stop();
  static void destroy();

  asio::io_context &context();

  UserManager &user_manager();
  RoomManager &room_manager();
  Sqlite3 &database();
  Sqlite3 &gameDatabase();  // gamedb的getter
  Shell &shell();

  void sendEarlyPacket(ClientSocket &client, const std::string_view &type, const std::string_view &msg);

  RoomThread &createThread();
  void removeThread(int threadId);
  std::weak_ptr<RoomThread> getThread(int threadId);
  RoomThread &getAvailableThread();
  const std::unordered_map<int, std::shared_ptr<RoomThread>> &getThreads() const;

  std::thread::id mainThreadId() const;

  void broadcast(const std::string_view &command, const std::string_view &jsonData);

  const ServerConfig &config() const;
  void reloadConfig();
  bool checkBanWord(const std::string_view &str);

  void temporarilyBan(int playerId);
  bool isTempBanned(const std::string_view &addr) const;

  void beginTransaction();
  void endTransaction();

  const std::string &getMd5() const;
  void refreshMd5();

  int64_t getUptime() const;

  bool nameIsInWhiteList(const std::string_view &name) const;

private:
  explicit Server();
  std::unique_ptr<ServerConfig> m_config;
  std::unique_ptr<ServerSocket> m_socket;

  std::unique_ptr<Sqlite3> db;
  std::unique_ptr<Sqlite3> gamedb;  // 存档变量
  std::mutex transaction_mutex;

  std::unordered_map<int, std::shared_ptr<RoomThread>> m_threads;

  std::unique_ptr<UserManager> m_user_manager;
  std::unique_ptr<RoomManager> m_room_manager;

  std::unique_ptr<Shell> m_shell;

  asio::io_context *main_io_ctx = nullptr;
  std::thread::id main_thread_id;

  std::vector<std::string> temp_banlist;

  std::string md5;

  int64_t start_timestamp;
  std::unique_ptr<asio::steady_timer> heartbeat_timer;

  void startHeartbeat();

  void _refreshMd5();
};
