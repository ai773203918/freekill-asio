// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class ServerSocket;
class ClientSocket;

class UserManager;
class RoomManager;
class RoomThread;

class Shell;

using asio::ip::tcp;

class Server {
public:
  static Server &instance();
  Server(Server &) = delete;
  Server(Server &&) = delete;
  ~Server();

  void listen(asio::io_context &io_ctx, tcp::endpoint end);
  void stop();
  static void destroy();

  UserManager &user_manager();
  RoomManager &room_manager();
  Shell &shell();

  void sendEarlyPacket(ClientSocket &client, const std::string_view &type, const std::string_view &msg);

  RoomThread &createThread();
  void removeThread(int threadId);
  RoomThread *getThread(int threadId);
  RoomThread &getAvailableThread();

  /*
  Sqlite3 *getDatabase();

  void broadcast(const QByteArray &command, const QByteArray &jsonData);
  bool isListening;

  QJsonValue getConfig(const QString &command);
  */
  bool checkBanWord(const std::string_view &str);
  /*
  void temporarilyBan(int playerId);

  void beginTransaction();
  void endTransaction();

  const QString &getMd5() const;
  void refreshMd5();

  qint64 getUptime() const;

  bool nameIsInWhiteList(const QString &name) const;
  */

private:
  explicit Server();
  std::unique_ptr<ServerSocket> m_socket;

  std::unique_ptr<UserManager> m_user_manager;
  std::unique_ptr<RoomManager> m_room_manager;
  std::unordered_map<int, std::unique_ptr<RoomThread>> m_threads;

  std::unique_ptr<Shell> m_shell;

  asio::io_context *main_io_ctx = nullptr;

  /*
  QList<QString> temp_banlist;

  Sqlite3 *db;
  QMutex transaction_mutex;
  QString md5;

  QElapsedTimer uptime_counter;

  void readConfig();
  QJsonObject config;

  bool hasWhitelist = false;
  QVariantList whitelist;
  */
};
