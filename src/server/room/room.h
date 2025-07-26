// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "server/room/roombase.h"

class Server;
class Player;
class RoomThread;

/**
  @brief Server类负责管理游戏服务端的运行。

  该类负责表示游戏房间，与大厅进行交互以调整玩家
*/
class Room : public RoomBase {
public:
  explicit Room(RoomThread *m_thread);
  // ~Room();

  void addPlayer(Player *player);
  void removePlayer(Player *player);
  void handlePacket(Player *sender, const Packet &packet);

  /*
  // Property reader & setter
  // ==================================={
  int getId() const;
  void setId(int id);
  QString getName() const;
  void setName(const QString &name);
  int getCapacity() const;
  void setCapacity(int capacity);
  bool isFull() const;
  const QJsonObject getSettingsObject() const;
  const QByteArray getSettings() const;
  void setSettings(QByteArray settings);
  bool isAbandoned() const;

  Player *getOwner() const;
  void setOwner(Player *owner);

  void addRobot(Player *player);

  void addObserver(Player *player);
  void removeObserver(Player *player);
  QList<Player *> getObservers() const;
  bool hasObserver(Player *player) const;

  int getTimeout() const;
  void setTimeout(int timeout);
  void delay(int ms);

  bool isOutdated();

  bool isStarted() const;
  // ====================================}

  void updatePlayerWinRate(int id, const QString &mode, const QString &role, int result);
  void updateGeneralWinRate(const QString &general, const QString &mode, const QString &role, int result);

  void gameOver();
  void manuallyStart();
  void pushRequest(const QString &req);

  void addRejectId(int id);
  void removeRejectId(int id);

  // router用

  void setRequestTimer(int ms);
  void destroyRequestTimer();

  // FIXME
  volatile bool insideGameOver = false;

  // Lua专用
  int getRefCount();
  void increaseRefCount();
  void decreaseRefCount();

 signals:
  void abandoned();

  void playerAdded(Player *player);
  void playerRemoved(Player *player);
  */

private:
  /*
  int id;               // Lobby's id is 0
  QString name;         // “阴间大乱斗”
  int capacity;         // by default is 5, max is 8
  QByteArray settings;  // JSON string
  QJsonObject settings_obj;  // JSON object
  bool m_abandoned;     // If room is empty, delete it

  Player *owner;  // who created this room?
  QList<int> runned_players;
  QList<int> rejected_players;
  int robot_id;
  bool gameStarted;
  bool m_ready;

  int timeout;
  QString md5;

  int lua_ref_count = 0; ///< Lua引用计数，当Room为abandon时，只要lua中还有计数，就不可删除
  QMutex lua_ref_mutex;

  QTimer *request_timer = nullptr;

  void addRunRate(int id, const QString &mode);
  void updatePlayerGameData(int id, const QString &mode);
  */

  // handle packet
  void quitRoom(Player *, const Packet &);
  void addRobotRequest(Player *, const Packet &);
  void kickPlayer(Player *, const Packet &);
  void ready(Player *, const Packet &);
  void startGame(Player *, const Packet &);
};
