// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/room/roombase.h"
#include "server/room/lobby.h"
#include "server/room/room.h"
#include "server/server.h"
#include "server/room/room_manager.h"
#include "server/user/user_manager.h"
#include "server/user/player.h"
#include "network/client_socket.h"

bool RoomBase::isLobby() const {
  return dynamic_cast<const Lobby *>(this) != nullptr;
}

int RoomBase::getId() const { return id; }

/*
QList<ServerPlayer *> RoomBase::getPlayers() const { return players; }

QList<ServerPlayer *> RoomBase::getOtherPlayers(ServerPlayer *expect) const {
  QList<ServerPlayer *> others = getPlayers();
  others.removeOne(expect);
  return others;
}

ServerPlayer *RoomBase::findPlayer(int id) const {
  for (auto p : players) {
    if (p->getId() == id)
      return p;
  }
  return nullptr;
}
*/

void RoomBase::doBroadcastNotify(const std::vector<int> targets,
                                 const std::string_view &command, const std::string_view &cborData) {
  auto &um = Server::instance().user_manager();
  for (auto connId : targets) {
    auto p = um.findPlayerByConnId(connId);
    if (p) p->doNotify(command, cborData);
  }
}

void RoomBase::chat(Player &sender, const Packet &packet) {
  auto &server = Server::instance();
  auto &um = server.user_manager();
  auto data = packet.cborData;
  // auto doc = String2Json(cborData).object();
  // auto type = doc["type"].toInt();
  // doc["sender"] = sender->getId();

  struct cbor_load_result result;
  auto mp = cbor_load((cbor_data)data.data(), data.size(), &result);
  if (!cbor_isa_map(mp)) {
    cbor_decref(&mp);
    return;
  }

  auto sz = cbor_map_size(mp);
  auto pairs = cbor_map_handle(mp);

  int type;
  std::string msg;
  auto senderId = sender.getId();

  for (size_t i = 0; i < sz; i++) {
    auto pair = pairs[i];
    auto k = pair.key, v = pair.value;

    if (cbor_isa_string(k) && strncmp((const char *)cbor_string_handle(k), "msg", 3) == 0) {
      if (!cbor_isa_string(v)) continue;
      msg = std::string { (const char *)cbor_string_handle(v), cbor_string_length(v) };
    }

    if (cbor_isa_string(k) && strncmp((const char *)cbor_string_handle(k), "type", 4) == 0) {
      if (!cbor_isa_uint(v)) continue;
      type = cbor_get_uint8(v);
    }
  }

  // 好了mp没用了 我们必须发新map给client
  cbor_decref(&mp);

  if (!server.checkBanWord(msg)) {
    return;
  }

  // 300字限制，与客户端相同 STL必须先判长度
  if (msg.size() > 300)
    msg.erase(msg.begin() + 300, msg.end());

  // 新map: { type, sender, msg, [userName] }

  // 还是来拼好cbor吧 前有手搓cbor
  std::ostringstream oss;
  char uint_buf[10]; size_t uint_len;
  uint_len = cbor_encode_uint(senderId, (cbor_mutable_data)uint_buf, 10);

  if (type == 1) {
    oss << "\xA4\x64type\x01\x66sender" << std::string_view { uint_buf, uint_len }
      << "\x68userName";

    uint_len = cbor_encode_uint(sender.getScreenName().size(), (cbor_mutable_data)uint_buf, 10);
    uint_buf[0] += 0x60; // uint(n) -> str(#)
    oss << std::string_view { uint_buf, uint_len } << sender.getScreenName()
      << "\x63msg";

    uint_len = cbor_encode_uint(msg.size(), (cbor_mutable_data)uint_buf, 10);
    uint_buf[0] += 0x60; // uint(n) -> str(#)
    oss << std::string_view { uint_buf, uint_len } << msg;

    auto lobby = dynamic_cast<Lobby *>(this);
    for (auto &[pid, _] : lobby->getPlayers()) {
      auto p = um.findPlayerByConnId(pid);
      if (p) p->doNotify("Chat", oss.str());
    }
  } else {
    oss << "\xA3\x64type\x02\x66sender" << std::string_view { uint_buf, uint_len }
      << "\x63msg";

    uint_len = cbor_encode_uint(msg.size(), (cbor_mutable_data)uint_buf, 10);
    uint_buf[0] += 0x60; // uint(n) -> str(#)
    oss << std::string_view { uint_buf, uint_len } << msg;

    auto room = dynamic_cast<Room *>(this);
    room->doBroadcastNotify(room->getPlayers(), "Chat", oss.str());
    room->doBroadcastNotify(room->getObservers(), "Chat", oss.str());
  }

  spdlog::info("[Chat/{}] {}: {}",
        isLobby() ? "Lobby" : fmt::format("#{}", dynamic_cast<Room *>(this)->getId()),
        sender.getScreenName(),
        msg);
}
