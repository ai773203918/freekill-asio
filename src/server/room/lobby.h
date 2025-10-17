// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "server/room/roombase.h"

struct Packet;
class Player;

class Lobby final : public RoomBase {
private:
  // connId -> true
  std::unordered_map<int, bool> players;

public:
  Lobby();
  Lobby(Lobby &) = delete;
  Lobby(Lobby &&) = delete;

  auto getPlayers() const -> const decltype(players) &;

  void addPlayer(Player &player) final;
  void removePlayer(Player &player) final;
  void handlePacket(Player &sender, const Packet &packet) final;

  void updateOnlineInfo();

  void checkAbandoned();

private:
  // for handle packet
  void updateAvatar(Player &, const Packet &);
  void updatePassword(Player &, const Packet &);
  void createRoom(Player &, const Packet &);
  void enterRoom(Player &, const Packet &);
  void observeRoom(Player &, const Packet &);
  void refreshRoomList(Player &, const Packet &);

  void joinRoom(Player &, const Packet &, bool ob = false);
};
