#pragma once

#include "server/room/roombase.h"

struct Packet;
class Player;

class Lobby : public RoomBase {
public:
  Lobby();

  void addPlayer(Player &player);
  void removePlayer(Player &player);
  void handlePacket(Player &sender, const Packet &packet);

private:
  // for handle packet
  void updateAvatar(Player &, const Packet &);
  void updatePassword(Player &, const Packet &);
  void createRoom(Player &, const Packet &);
  void getRoomConfig(Player &, const Packet &);
  void enterRoom(Player &, const Packet &);
  void observeRoom(Player &, const Packet &);
  void refreshRoomList(Player &, const Packet &);

  std::unordered_map<int, bool> players;
};
