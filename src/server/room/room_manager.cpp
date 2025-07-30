// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/room/room_manager.h"
#include "server/room/room.h"
#include "server/room/roombase.h"
#include "server/room/lobby.h"
#include "server/user/player.h"
#include "server/gamelogic/roomthread.h"
#include "server/server.h"

RoomManager::RoomManager() {
  m_lobby = std::make_unique<Lobby>();
}

Room *RoomManager::createRoom(Player &creator, const std::string &name, int capacity,
                int timeout, const std::string &settings)
{
  auto &server = Server::instance();
  if (!server.checkBanWord(name)) {
    creator.doNotify("ErrorMsg", "unk error");
    return nullptr;
  }

  auto &thread = server.getAvailableThread();

  auto room = std::make_unique<Room>();
  auto id = room->getId();

  rooms[id] = std::move(room);
  auto &r = rooms[id];
  r->setName(name);
  r->setCapacity(capacity);
  // r->setThread(thread);
  r->setTimeout(timeout);
  r->setSettings(settings);
  return r.get();
}

void RoomManager::removeRoom(int id) {
}

RoomBase *RoomManager::findRoom(int id) const {
  if (id == 0) return m_lobby.get();
  auto it = rooms.find(id);
  if (it != rooms.end()) {
    return it->second.get();
  } else {
    return nullptr;
  }
}

Lobby &RoomManager::lobby() const {
  return *m_lobby;
}

const std::map<int, std::unique_ptr<Room>> &RoomManager::getRooms() const {
  return rooms;
}
