// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

class Room;
class RpcLua;

class RoomThread {
public:
  explicit RoomThread(asio::io_context &main_ctx);
  RoomThread(RoomThread &) = delete;
  RoomThread(RoomThread &&) = delete;
  ~RoomThread();

  int id() const;
  asio::io_context &context();

  void quit();

  // signal emitters
  void pushRequest(const std::string &req);
  void delay(int roomId, int ms);
  void wakeUp(int roomId, const char *reason);

  void setPlayerState(int connId, int roomId);
  void addObserver(int connId, int roomId);
  void removeObserver(int connId, int roomId);

  /*
  bool isFull() const;

  int getCapacity() const { return m_capacity; }
  QString getMd5() const;
  Room *getRoom(int id) const;

  bool isOutdated();

  LuaInterface *getLua() const;

 public slots:
  void onRoomAbandoned();
  */

private:
  int m_id = 0;

  int evt_fd;
  asio::io_context io_ctx;
  asio::io_context &main_io_ctx;
  std::thread m_thread;

  std::vector<int> m_rooms;

  // 缺少一个Qt家的全局timer 必须自己维护一个
  asio::steady_timer timer;

  std::unique_ptr<RpcLua> L;

  void start();

  // signals
  std::function<void(const std::string& req)> push_request_callback = nullptr;
  std::function<void(int roomId, int ms)> delay_callback = nullptr;
  std::function<void(int roomId, const char* reason)> wake_up_callback = nullptr;
  std::function<void(int connId, int roomId)> set_player_state_callback = nullptr;
  std::function<void(int connId, int roomId)> add_observer_callback = nullptr;
  std::function<void(int connId, int roomId)> remove_observer_callback = nullptr;

  void emit_signal(std::function<void()> f);

  /*
  int m_capacity;
  QString md5;
  */
};
