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
  explicit Room();
  Room(Room &) = delete;
  Room(Room &&) = delete;
  ~Room();

  void addPlayer(Player &player);
  void removePlayer(Player &player);
  void handlePacket(Player &sender, const Packet &packet);

  // Property reader & setter
  // ==================================={
  std::string &getName();
  void setName(const std::string_view &name);
  int getCapacity() const;
  void setCapacity(int capacity);
  bool isFull() const;

  const std::vector<int> &getPlayers() const;

  // TODO 改成用得到的password和gameMode
  // const QJsonObject getSettingsObject() const;
  const std::string_view getSettings() const;
  const std::string_view getGameMode() const;
  const std::string_view getPassword() const;
  void setSettings(const std::string_view &settings);
  bool isAbandoned() const;

  Player *getOwner() const;
  void setOwner(Player &owner);

  void addRobot(Player &player);

  void addObserver(Player &player);
  void removeObserver(Player &player);
  bool hasObserver(Player &player) const;
  const std::vector<int> &getObservers() const;

  int getTimeout() const;
  void setTimeout(int timeout);
  void delay(int ms);

  bool isOutdated();

  bool isStarted() const;
  // ====================================}

  /*
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
  */

  // Lua专用
  int getRefCount();
  void increaseRefCount();
  void decreaseRefCount();

  /*
 signals:
  void abandoned();

  void playerAdded(Player *player);
  void playerRemoved(Player *player);
  */

private:
  int m_thread_id = 0;

  // connId[]
  std::vector<int> players;
  std::vector<int> observers;

  std::string name;         // “阴间大乱斗”
  int capacity = 0;         // by default is 5, max is 8
  int m_owner_conn_id = 0;

  std::string settings;
  std::string gameMode;
  std::string password;
  bool m_abandoned;     // If room is empty, delete it

  std::vector<int> runned_players;
  std::vector<int> rejected_players;
  bool gameStarted = false;
  bool m_ready = false;

  int timeout = 0;
  // QString md5;

  int lua_ref_count = 0; // Lua引用计数，当Room被abandon时，若lua有计数就不可删除
  std::mutex lua_ref_mutex;

  // QTimer *request_timer = nullptr;

  void addRunRate(int id, const std::string_view &mode);
  void updatePlayerGameData(int id, const std::string_view &mode);

  void setPlayerReady(Player &, bool ready);

  // handle packet
  void quitRoom(Player &, const Packet &);
  void addRobotRequest(Player &, const Packet &);
  void kickPlayer(Player &, const Packet &);
  void ready(Player &, const Packet &);
  void startGame(Player &, const Packet &);
};
