#include "server/room/room_manager.h"
#include "server/room/room.h"
#include "server/room/roombase.h"
#include "server/room/lobby.h"

RoomManager::RoomManager() {
  m_lobby = std::make_unique<Lobby>();
}

void RoomManager::createRoom(Player &owner, const std::string &name, int capacity,
                int timeout, const std::string &settings)
{
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


