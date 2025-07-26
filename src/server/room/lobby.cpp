#include "server/room/lobby.h"
#include "server/server.h"
#include "server/user/player.h"
#include "network/client_socket.h"

Lobby::Lobby() {
  id = 0;
}

void Lobby::addPlayer(Player &player) {
  players.push_back(std::string(player.getConnId()));
  player.setRoom(*this);

  // if (player->getState() == Player::Robot) {
  //   removePlayer(player);
  //   player->deleteLater();
  // } else {
  //   player->doNotify("EnterLobby", QCborValue().toCbor());
  // }

  // server->updateOnlineInfo();
}

void Lobby::removePlayer(Player &player) {
//   players.removeOne(player);
//   server->updateOnlineInfo();
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

void Lobby::createRoom(Player *sender, const QString &jsonData) {
  auto arr = String2Json(jsonData).array();
  auto name = arr[0].toString();
  auto capacity = arr[1].toInt();
  auto timeout = arr[2].toInt();
  auto settings =
    QJsonDocument(arr[3].toObject()).toJson(QJsonDocument::Compact);
  ServerInstance->createRoom(sender, name, capacity, timeout, settings);
}

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

void Lobby::enterRoom(Player *sender, const QString &jsonData) {
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
        room->addPlayer(sender);
      }
    } else {
      sender->doNotify("ErrorMsg", "room password error");
    }
  } else {
    sender->doNotify("ErrorMsg", "no such room");
  }
}

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

void Lobby::refreshRoomList(Player *sender, const QString &) {
  ServerInstance->updateRoomList(sender);
};
*/

typedef void (Lobby::*room_cb)(Player &, const Packet &);

void Lobby::handlePacket(Player &sender, const Packet &packet) {
  static const std::unordered_map<std::string_view, room_cb> lobby_actions = {
    // {"UpdateAvatar", &Lobby::updateAvatar},
    // {"UpdatePassword", &Lobby::updatePassword},
    // {"CreateRoom", &Lobby::createRoom},
    // {"GetRoomConfig", &Lobby::getRoomConfig},
    // {"EnterRoom", &Lobby::enterRoom},
    // {"ObserveRoom", &Lobby::observeRoom},
    // {"RefreshRoomList", &Lobby::refreshRoomList},
    // {"Chat", &Lobby::chat},
  };

  auto func = lobby_actions.at(packet.command);
  if (func) (this->*func)(sender, packet);
}
