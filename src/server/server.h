// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class Sqlite3;
class AuthManager;
class ServerSocket;
class ClientSocket;
class ServerPlayer;
class RoomThread;
class Lobby;

class Server {
public:
  explicit Server(QObject *parent = nullptr);
  ~Server();

  bool listen(const QHostAddress &address = QHostAddress::Any,
              ushort port = 9527u);

  void createRoom(ServerPlayer *owner, const QString &name, int capacity,
                  int timeout = 15, const QByteArray &settings = QByteArrayLiteral("{}"));

  void removeRoom(int id);

  Room *findRoom(int id) const;
  Lobby *lobby() const;

  ServerPlayer *findPlayer(int id) const;
  ServerPlayer *findPlayerByConnId(const QString &connId) const;
  void addPlayer(ServerPlayer *player);
  void removePlayer(int id);
  void removePlayerByConnId(QString connid);
  auto getPlayers() { return players; }

  void updateRoomList(ServerPlayer *teller);
  void updateOnlineInfo();

  Sqlite3 *getDatabase();

  void broadcast(const QByteArray &command, const QByteArray &jsonData);
  void sendEarlyPacket(ClientSocket *client, const QByteArray &type, const QByteArray &msg);
  void createNewPlayer(ClientSocket *client, const QString &name, const QString &avatar, int id, const QString &uuid_str);
  void setupPlayer(ServerPlayer *player, bool all_info = true);
  bool isListening;

  QJsonValue getConfig(const QString &command);
  bool checkBanWord(const QString &str);
  void temporarilyBan(int playerId);

  void beginTransaction();
  void endTransaction();

  const QString &getMd5() const;
  void refreshMd5();

  qint64 getUptime() const;

  bool nameIsInWhiteList(const QString &name) const;

signals:
  void roomCreated(Room *room);
  void playerAdded(ServerPlayer *player);
  void playerRemoved(ServerPlayer *player);

public slots:
  void processNewConnection(ClientSocket *client);

private:
  friend class Shell;
  ServerSocket *server;

  Lobby *m_lobby;
  QMap<int, Room *> rooms;
  int nextRoomId;
  friend Room::Room(RoomThread *m_thread);
  QHash<int, ServerPlayer *> players;
  QHash<QString, ServerPlayer *> players_conn;
  QList<QString> temp_banlist;

  AuthManager *auth;
  Sqlite3 *db;
  QMutex transaction_mutex;
  QString md5;

  QElapsedTimer uptime_counter;

  void readConfig();
  QJsonObject config;

  bool hasWhitelist = false;
  QVariantList whitelist;
};

extern Server *ServerInstance;
