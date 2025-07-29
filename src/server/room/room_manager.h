#pragma once

class RoomBase;
class Lobby;
class Room;
class Player;

class RoomManager {
public:
  explicit RoomManager();
  RoomManager(RoomManager &) = delete;
  RoomManager(RoomManager &&) = delete;

  Room *createRoom(Player &, const std::string &name, int capacity,
                  int timeout = 15, const std::string &settings = "\xA0");

  void removeRoom(int id);

  RoomBase *findRoom(int id) const;
  Lobby &lobby() const;

private:
  std::unique_ptr<Lobby> m_lobby;
  std::unordered_map<int, std::unique_ptr<Room>> rooms;
};
