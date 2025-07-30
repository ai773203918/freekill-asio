// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class LuaInterface;
class Room;
class Server;
class ServerPlayer;
class RoomThread;
class RpcLua;

class Scheduler {
 public:
  /*
  explicit Scheduler(RoomThread *m_thread);
  ~Scheduler();

  auto getLua() const { return L; }

 public slots:
  void handleRequest(const QString &req);
  void doDelay(int roomId, int ms);
  void resumeRoom(int roomId, const char *reason);

  // 只在rpc模式下有效果
  void setPlayerState(const QString &, int roomId);
  void addObserver(const QString &, int roomId);
  void removeObserver(const QString &, int roomId);

 private:
  LuaInterface *L;
  */
};

class RoomThread {
public:
  explicit RoomThread(asio::io_context &main_ctx);
  RoomThread(RoomThread &) = delete;
  RoomThread(RoomThread &&) = delete;
  ~RoomThread();

  int id() const;
  asio::io_context &context();

  void quit();

  /*

  Server *getServer() const;
  bool isFull() const;

  int getCapacity() const { return m_capacity; }
  QString getMd5() const;
  Room *getRoom(int id) const;

  bool isConsoleStart() const;

  bool isOutdated();

  LuaInterface *getLua() const;

 signals:
  void scheduler_ready();
  void pushRequest(const QString &req);
  void delay(int roomId, int ms);
  void wakeUp(int roomId, const char *);

  // 只在rpc模式下有效果
  void setPlayerState(const QString &, int roomId);
  void addObserver(const QString &, int roomId);
  void removeObserver(const QString &, int roomId);

 public slots:
  void onRoomAbandoned();
  */

private:
  int m_id = 0;

  std::thread m_thread;
  int evt_fd;
  asio::io_context io_ctx;
  asio::io_context &main_io_ctx;

  std::vector<int> m_rooms;

  std::unique_ptr<RpcLua> L;

  void start();
  /*
  int m_capacity;
  QString md5;

  Scheduler *m_scheduler;
  */
};
