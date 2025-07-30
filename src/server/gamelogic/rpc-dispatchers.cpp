// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/gamelogic/rpc-dispatchers.h"
#include "server/server.h"
#include "server/user/user_manager.h"
#include "server/user/player.h"
#include "server/room/room_manager.h"
#include "server/room/room.h"

// 这何尝不是一种手搓swig。。

using namespace JsonRpc;
using _rpcRet = std::pair<bool, JsonRpcParam>;

static JsonRpcParam nullVal;

// part1: stdout相关

static _rpcRet _rpc_qDebug(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 && std::holds_alternative<std::string_view>(packet.param1) )) {
    return { false, nullVal };
  }

  spdlog::debug("{}", std::get<std::string_view>(packet.param1));
  return { true, nullVal };
}

static _rpcRet _rpc_qInfo(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 && std::holds_alternative<std::string_view>(packet.param1) )) {
    return { false, nullVal };
  }

  spdlog::info("{}", std::get<std::string_view>(packet.param1));
  return { true, nullVal };
}

static _rpcRet _rpc_qWarning(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 && std::holds_alternative<std::string_view>(packet.param1) )) {
    return { false, nullVal };
  }

  spdlog::warn("{}", std::get<std::string_view>(packet.param1));
  return { true, nullVal };
}

static _rpcRet _rpc_qCritical(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 && std::holds_alternative<std::string_view>(packet.param1) )) {
    return { false, nullVal };
  }

  spdlog::critical("{}", std::get<std::string_view>(packet.param1));
  return { true, nullVal };
}

static _rpcRet _rpc_print(const JsonRpcPacket &packet) {
  for (int i = 0; i < packet.param_count; i++) {
    switch (i) {
      case 0:
        std::cout << std::get<std::string_view>(packet.param1) << '\t';
        break;
      case 1:
        std::cout << std::get<std::string_view>(packet.param2) << '\t';
        break;
      case 2:
        std::cout << std::get<std::string_view>(packet.param3) << '\t';
        break;
      case 3:
        std::cout << std::get<std::string_view>(packet.param4) << '\t';
        break;
      case 4:
        std::cout << std::get<std::string_view>(packet.param5) << '\t';
        break;
    }
  }
  std::cout << std::endl;
  return { true, nullVal };
}

// part2: Player相关

static _rpcRet _rpc_Player_doRequest(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 5 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<std::string_view>(packet.param2) &&
    std::holds_alternative<std::string_view>(packet.param3) &&
    std::holds_alternative<int>(packet.param4) &&
    std::holds_alternative<int64_t>(packet.param5)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  auto command = std::get<std::string_view>(packet.param2);
  auto jsonData = std::get<std::string_view>(packet.param3);
  auto timeout = std::get<int>(packet.param4);
  int64_t timestamp = std::get<int64_t>(packet.param5);

  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  // TODO 别急
  // player->doRequest(command, jsonData, timeout, timestamp);

  return { true, nullVal };
}

static _rpcRet _rpc_Player_waitForReply(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 2 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<int>(packet.param2)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  auto timeout = std::get<int>(packet.param2);

  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  // TODO 别急
  // auto reply = player->waitForReply(timeout);
  return { true, "" }; // reply };
}

static _rpcRet _rpc_Player_doNotify(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 3 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<std::string_view>(packet.param2) &&
    std::holds_alternative<std::string_view>(packet.param3)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  auto command = std::get<std::string_view>(packet.param2);
  auto jsonData = std::get<std::string_view>(packet.param3);

  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  player->doNotify(command, jsonData);

  return { true, nullVal };
}

static _rpcRet _rpc_Player_thinking(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  bool isThinking = player->thinking();
  return { true, isThinking };
}

static _rpcRet _rpc_Player_setThinking(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 2 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<bool>(packet.param2)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  bool thinking = std::get<bool>(packet.param2);

  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  player->setThinking(thinking);
  return { true, nullVal };
}

static _rpcRet _rpc_Player_setDied(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 2 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<bool>(packet.param2)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  bool died = std::get<bool>(packet.param2);

  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  player->setDied(died);
  return { true, nullVal };
}

static _rpcRet _rpc_Player_emitKick(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }

  auto connId = std::get<int>(packet.param1);
  auto player = Server::instance().user_manager().findPlayerByConnId(connId);
  if (!player) {
    return { false, "Player not found" };
  }

  // TODO 别急
  // player->emitKicked();
  return { true, nullVal };
}

// part3: Room相关

static _rpcRet _rpc_Room_delay(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 2 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<int>(packet.param2)
  )) {
    return { false, nullVal };
  }
  int id = std::get<int>(packet.param1);
  int ms = std::get<int>(packet.param2);
  if (ms <= 0) {
    return { false, nullVal };
  }
  auto room = Server::instance().room_manager().findRoom(id);
  if (!room) {
    return { false, "Room not found" };
  }

  // TODO
  // room->delay(ms);

  return { true, nullVal };
}

static _rpcRet _rpc_Room_updatePlayerWinRate(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 5 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<int>(packet.param2) &&
    std::holds_alternative<std::string_view>(packet.param3) &&
    std::holds_alternative<std::string_view>(packet.param4) &&
    std::holds_alternative<int>(packet.param5)
  )) {
    return { false, nullVal };
  }

  int roomId = std::get<int>(packet.param1);
  int playerId = std::get<int>(packet.param2);
  auto mode = std::get<std::string_view>(packet.param3);
  auto role = std::get<std::string_view>(packet.param4);
  int result = std::get<int>(packet.param5);

  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    return { false, "Room not found" };
  }

  // TODO
  // room->updatePlayerWinRate(playerId, mode, role, result);

  return { true, nullVal };
}

static _rpcRet _rpc_Room_updateGeneralWinRate(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 5 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<std::string_view>(packet.param2) &&
    std::holds_alternative<std::string_view>(packet.param3) &&
    std::holds_alternative<std::string_view>(packet.param4) &&
    std::holds_alternative<int>(packet.param5)
  )) {
    return { false, nullVal };
  }

  int roomId = std::get<int>(packet.param1);
  auto general = std::get<int>(packet.param2);
  auto mode = std::get<int>(packet.param3);
  auto role = std::get<int>(packet.param4);
  int result = std::get<int>(packet.param5);

  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    return { false, "Room not found" };
  }

  // room->updateGeneralWinRate(general, mode, role, result);

  return { true, nullVal };
}

static _rpcRet _rpc_Room_gameOver(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }

  int roomId = std::get<int>(packet.param1);
  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    return { false, "Room not found" };
  }

  // room->gameOver();

  return { true, nullVal };
}

static _rpcRet _rpc_Room_setRequestTimer(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 2 &&
    std::holds_alternative<int>(packet.param1) &&
    std::holds_alternative<int>(packet.param2)
  )) {
    return { false, nullVal };
  }

  int id = std::get<int>(packet.param1);
  int ms = std::get<int>(packet.param2);
  if (ms <= 0) {
    return { false, nullVal };
  }

  auto room = Server::instance().room_manager().findRoom(id);
  if (!room) {
    return { false, "Room not found" };
  }

  // room->setRequestTimer(ms);

  return { true, nullVal };
}

static _rpcRet _rpc_Room_destroyRequestTimer(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }

  int roomId = std::get<int>(packet.param1);
  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    return { false, "Room not found" };
  }

  // room->destroyRequestTimer();

  return { true, nullVal };
}

static _rpcRet _rpc_Room_increaseRefCount(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }

  int roomId = std::get<int>(packet.param1);
  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    return { false, "Room not found" };
  }

  // room->increaseRefCount();

  return { true, nullVal };
}

static _rpcRet _rpc_Room_decreaseRefCount(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }

  int roomId = std::get<int>(packet.param1);
  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    return { false, "Room not found" };
  }

  // room->decreaseRefCount();

  return { true, nullVal };
}

// 收官：getRoom

/*
static QCborMap getPlayerObject(Player *p) {
  JsonRpcPacket gameData;
  for (auto i : p->getGameData()) gameData << i;

  return {
    { "connId", p->getConnId() },
    { "id", p->getId() },
    { "screenName", p->getScreenName() },
    { "avatar", p->getAvatar() },
    { "totalGameTime", p->getTotalGameTime() },

    { "state", p->getState() },

    { "gameData", gameData },
  };
}
*/

static _rpcRet _rpc_RoomThread_getRoom(const JsonRpcPacket &packet) {
  if (!( packet.param_count == 1 &&
    std::holds_alternative<int>(packet.param1)
  )) {
    return { false, nullVal };
  }
  int id = std::get<int>(packet.param1);
  if (id <= 0) {
    return { false, nullVal };
  }

  auto room = Server::instance().room_manager().findRoom(id);
  if (!room) {
    return { false, "Room not found" };
  }

  // JsonRpcPacket players;
  // for (auto p : room->getPlayers()) {
  //   players << getPlayerObject(p);
  // }

  // QCborMap ret {
  //   { "id", room->getId() },
  //   { "players", players },
  //   { "ownerId", room->getOwner()->getId() },
  //   { "timeout", room->getTimeout() },

  //   { "settings", room->getSettings().constData() },
  // };
  // return { true, ret };
  return { true, nullVal };
}

const JsonRpc::RpcMethodMap ServerRpcMethods {
  { "qDebug", _rpc_qDebug },
  { "qInfo", _rpc_qInfo },
  { "qWarning", _rpc_qWarning },
  { "qCritical", _rpc_qCritical },
  { "print", _rpc_print },

  { "ServerPlayer_doRequest", _rpc_Player_doRequest },
  { "ServerPlayer_waitForReply", _rpc_Player_waitForReply },
  { "ServerPlayer_doNotify", _rpc_Player_doNotify },
  { "ServerPlayer_thinking", _rpc_Player_thinking },
  { "ServerPlayer_setThinking", _rpc_Player_setThinking },
  { "ServerPlayer_setDied", _rpc_Player_setDied },
  { "ServerPlayer_emitKick", _rpc_Player_emitKick },

  { "Room_delay", _rpc_Room_delay },
  { "Room_updatePlayerWinRate", _rpc_Room_updatePlayerWinRate },
  { "Room_updateGeneralWinRate", _rpc_Room_updateGeneralWinRate },
  { "Room_gameOver", _rpc_Room_gameOver },
  { "Room_setRequestTimer", _rpc_Room_setRequestTimer },
  { "Room_destroyRequestTimer", _rpc_Room_destroyRequestTimer },
  { "Room_increaseRefCount", _rpc_Room_increaseRefCount },
  { "Room_decreaseRefCount", _rpc_Room_decreaseRefCount },

  { "RoomThread_getRoom", _rpc_RoomThread_getRoom },
};
