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

void RoomManager::createRoom(Player &owner, const std::string &name, int capacity,
                int timeout, const std::string &settings)
{
  auto &server = Server::instance();
  if (!server.checkBanWord(name)) {
    owner.doNotify("ErrorMsg", "unk error");
    return;
  }

  auto &thread = server.getAvailableThread();

  auto room = std::make_unique<Room>();
  auto id = room->getId();

  rooms[id] = std::move(room);
  auto &r = rooms[id];
  r->setName(name);
  r->setCapacity(capacity);
  // r->setThread(thread);
  // r.setTimeout(timeout);
  // r.setSettings(settings);
  // r.addPlayer(owner);
  // r.setOwner(owner);
}

void RoomManager::removeRoom(int id) {
}

RoomBase *RoomManager::findRoom(int id) const {
  if (id == 0) return m_lobby.get();
  return nullptr;
}

Lobby &RoomManager::lobby() const {
  return *m_lobby;
}


