// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/room/lobby.h"
#include "server/server.h"
#include "server/user/player.h"
#include "server/user/user_manager.h"
#include "server/room/room_manager.h"
#include "server/room/room.h"
#include "network/client_socket.h"

#include "core/c-wrapper.h"

// TODO: instance
Lobby::Lobby() {
  id = 0;
}

const std::unordered_map<int, bool> &Lobby::getPlayers() const {
  return players;
}

void Lobby::addPlayer(Player &player) {
  auto &um = Server::instance().user_manager();
  if (player.getState() == Player::Robot) {
    um.deletePlayer(player);
  } else {
    players[player.getConnId()] = true;
    player.setRoom(*this);
    player.doNotify("EnterLobby", "");
  }

  updateOnlineInfo();
}

void Lobby::removePlayer(Player &player) {
  auto connId = player.getConnId();
  players.erase(connId);
  updateOnlineInfo();
}

void Lobby::updateOnlineInfo() {
  auto &um = Server::instance().user_manager();
  auto arr = Cbor::encodeArray({
    players.size(),
    um.getPlayers().size(),
  });
  for (auto &[pid, _] : players) {
    auto p = um.findPlayerByConnId(pid);
    if (p) p->doNotify("UpdatePlayerNum", arr);
  }
}

/*
void Lobby::updateAvatar(Player *sender, const QString &jsonData) {
  auto arr = String2Json(jsonData).array();
  auto avatar = arr[0].toString();

  if (Sqlite3::checkString(avatar)) {
    auto sql = QString("UPDATE userinfo SET avatar='%1' WHERE id=%2;")
      .arg(avatar)
      .arg(sender->getId());
    ServerInstance->getDatabase()->exec(sql);
    sender->setAvatar(avatar);
    sender->doNotify("UpdateAvatar", avatar.toUtf8());
  }
}

void Lobby::updatePassword(Player *sender, const QString &jsonData) {
  auto arr = String2Json(jsonData).array();
  auto oldpw = arr[0].toString();
  auto newpw = arr[1].toString();
  auto sql_find =
    QString("SELECT password, salt FROM userinfo WHERE id=%1;")
    .arg(sender->getId());

  auto passed = false;
  auto arr2 = ServerInstance->getDatabase()->select(sql_find);
  auto result = arr2[0];
  passed = (result["password"] == QCryptographicHash::hash(
    oldpw.append(result["salt"]).toLatin1(),
    QCryptographicHash::Sha256)
  .toHex());
  if (passed) {
    auto sql_update =
      QString("UPDATE userinfo SET password='%1' WHERE id=%2;")
      .arg(QCryptographicHash::hash(
            newpw.append(result["salt"]).toLatin1(),
            QCryptographicHash::Sha256)
          .toHex())
      .arg(sender->getId());
    ServerInstance->getDatabase()->exec(sql_update);
  }

  sender->doNotify("UpdatePassword", passed ? "1" : "0");
}
*/

void Lobby::createRoom(Player &sender, const Packet &packet) {
  auto cbuf = (cbor_data)packet.cborData.data();
  auto len = packet.cborData.size();

  std::string_view name;
  int capacity = -1;
  int timeout = -1;
  std::string_view settings;
  size_t sz = 0;

  struct cbor_decoder_result decode_result;

  decode_result = cbor_stream_decode(cbuf, len, &Cbor::arrayCallbacks, &sz); // arr
  if (decode_result.read == 0) return;
  cbuf += decode_result.read; len -= decode_result.read;
  if (sz != 4) return;

  decode_result = cbor_stream_decode(cbuf, len, &Cbor::stringCallbacks, &name);
  if (decode_result.read == 0) return;
  cbuf += decode_result.read; len -= decode_result.read;
  if (name == "") return;

  decode_result = cbor_stream_decode(cbuf, len, &Cbor::intCallbacks, &capacity);
  if (decode_result.read == 0) return;
  cbuf += decode_result.read; len -= decode_result.read;
  if (capacity == -1) return;

  decode_result = cbor_stream_decode(cbuf, len, &Cbor::intCallbacks, &timeout);
  if (decode_result.read == 0) return;
  cbuf += decode_result.read; len -= decode_result.read;
  if (timeout == -1) return;

  settings = { (char *)cbuf, len };

  auto &rm = Server::instance().room_manager();
  auto room_ptr = rm.createRoom(sender, std::string(name), capacity, timeout, std::string(settings));
  if (room_ptr) {
    room_ptr->addPlayer(sender);
    if (sender.getRoom().getId() == room_ptr->getId()) {
      removePlayer(sender);
    }
  }
}

/*
void Lobby::getRoomConfig(Player *sender, const QString &jsonData) {
  auto arr = String2Json(jsonData).array();
  auto roomId = arr[0].toInt();
  auto room = ServerInstance->findRoom(roomId);
  if (room) {
    auto settings = room->getSettings();
    // 手搓JSON数组 跳过编码解码
    sender->doNotify("GetRoomConfig", QCborArray { roomId, settings }.toCborValue().toCbor());
  } else {
    sender->doNotify("ErrorMsg", "no such room");
  }
}
*/

void Lobby::enterRoom(Player &sender, const Packet &pkt) {
  auto &rm = Server::instance().room_manager();

  auto data = pkt.cborData;
  auto cbuf = (cbor_data)data.data();
  auto len = data.size();
  size_t sz;
  int roomId = 0;
  std::string_view pw;

  cbor_decoder_result result;
  result = cbor_stream_decode(cbuf, len, &Cbor::arrayCallbacks, &sz);
  if (result.read == 0) return;
  cbuf += result.read; len -= result.read;
  if (sz != 2) return;

  result = cbor_stream_decode(cbuf, len, &Cbor::intCallbacks, &roomId);
  if (result.read == 0) return;
  cbuf += result.read; len -= result.read;
  if (roomId == 0) return;

  result = cbor_stream_decode(cbuf, len, &Cbor::stringCallbacks, &pw);
  if (result.read == 0) return;
  cbuf += result.read; len -= result.read;

  auto room_ = rm.findRoom(roomId);
  if (room_) {
    auto &room = *dynamic_cast<Room *>(room_);
    auto password = room.getPassword();
    if (password.empty() || pw == password) {
      if (room.isOutdated()) {
        sender.doNotify("ErrorMsg", "room is outdated");
      } else {
        room.addPlayer(sender);
        if (sender.getRoom().getId() == room.getId()) {
          removePlayer(sender);
        }
      }
    } else {
      sender.doNotify("ErrorMsg", "room password error");
    }
  } else {
    sender.doNotify("ErrorMsg", "no such room");
  }
}

/*
void Lobby::observeRoom(Player *sender, const QString &jsonData) {
  auto arr = String2Json(jsonData).array();
  auto roomId = arr[0].toInt();
  auto room = ServerInstance->findRoom(roomId);
  if (room) {
    auto settings = QJsonDocument::fromJson(room->getSettings());
    auto password = settings["password"].toString();
    if (password.isEmpty() || arr[1].toString() == password) {
      if (room->isOutdated()) {
        sender->doNotify("ErrorMsg", "room is outdated");
      } else {
        room->addObserver(sender);
      }
    } else {
      sender->doNotify("ErrorMsg", "room password error");
    }
  } else {
    sender->doNotify("ErrorMsg", "no such room");
  }
}
*/

void Lobby::refreshRoomList(Player &sender, const Packet &) {
  auto &rm = Server::instance().room_manager();

  auto &rooms = rm.getRooms();

  // 拼好cbor 首先拼一个头
  std::ostringstream oss;

  size_t sz = rooms.size();
  char buf[10];
  size_t len = cbor_encode_uint(sz, (cbor_mutable_data)buf, 10);
  buf[0] += 0x80; // uint -> array header

  oss << std::string_view { buf, len };

  for (auto &[_, room] : rooms) {
    if (room->isFull()) continue;
    oss << Cbor::encodeArray({
      room->getId(),
      room->getName().data(),
      room->getGameMode().data(),
      room->getPlayers().size(),
      room->getCapacity(),
      !room->getPassword().empty(),
      room->isOutdated(),
    });
  }
  for (auto &[_, room] : rooms) {
    if (!room->isFull()) continue;
    oss << Cbor::encodeArray({
      room->getId(),
      room->getName().data(),
      room->getGameMode().data(),
      room->getPlayers().size(),
      room->getCapacity(),
      !room->getPassword().empty(),
      room->isOutdated(),
    });
  }
  sender.doNotify("UpdateRoomList", oss.str());
}

typedef void (Lobby::*room_cb)(Player &, const Packet &);

void Lobby::handlePacket(Player &sender, const Packet &packet) {
  static const std::unordered_map<std::string_view, room_cb> lobby_actions = {
    // {"UpdateAvatar", &Lobby::updateAvatar},
    // {"UpdatePassword", &Lobby::updatePassword},
    {"CreateRoom", &Lobby::createRoom},
    // {"GetRoomConfig", &Lobby::getRoomConfig},
    {"EnterRoom", &Lobby::enterRoom},
    // {"ObserveRoom", &Lobby::observeRoom},
    {"RefreshRoomList", &Lobby::refreshRoomList},
    {"Chat", &Lobby::chat},
  };

  auto iter = lobby_actions.find(packet.command);
  if (iter != lobby_actions.end()) {
    auto func = iter->second;
    (this->*func)(sender, packet);
  }
}
