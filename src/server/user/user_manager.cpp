#include "server/user/user_manager.h"
#include "server/user/player.h"
#include "server/user/auth.h"
#include "server/server.h"
#include "network/client_socket.h"

UserManager::UserManager() {
  m_auth = std::make_unique<AuthManager>();
}

Player &UserManager::findPlayer(int id) const {
  return *online_players_map.at(id).get();
}

Player &UserManager::findPlayerByConnId(const std::string_view connId) const {
  return *players_map.at(connId).get();
}

void UserManager::addPlayer(std::shared_ptr<Player> player) {
  int id = player->getId();
  if (id > 0) {
    if (online_players_map[id])
      online_players_map.erase(id);

    online_players_map[id] = player;
  }

  players_map[player->getConnId()] = player;
}

void UserManager::removePlayer(int id) {
  if (online_players_map[id]) {
    online_players_map.erase(id);
  }
}

void UserManager::removePlayerByConnId(const std::string_view connId) {
  if (players_map[connId]) {
    players_map.erase(connId);
  }
}

void UserManager::processNewConnection(std::shared_ptr<ClientSocket> client) {
  spdlog::info("client {} connected", client->peerAddress());

  auto &server = Server::instance();

  /*
  // check ban ip
  auto result = db->select(QString("SELECT * FROM banip WHERE ip='%1';").arg(addr));

  const char *errmsg = nullptr;

  if (!result.isEmpty()) {
    errmsg = "you have been banned!";
  } else if (temp_banlist.contains(addr)) {
    errmsg = "you have been temporarily banned!";
  } else if (players.count() >= getConfig("capacity").toInt()) {
    errmsg = "server is full!";
  }

  if (errmsg) {
    sendEarlyPacket(client, "ErrorDlg", errmsg);
    qInfo() << "Refused banned IP:" << addr;
    client->disconnectFromHost();
    return;
  }
  */

  // network delay test
  server.sendEarlyPacket(*client, "NetworkDelayTest", m_auth->getPublicKeyCbor());
  // Note: the client should send a setup string next
  // connect(client, &ClientSocket::message_got, auth, &AuthManager::processNewConnection);
  // client->timerSignup.start(30000);
}

void UserManager::createNewPlayer(std::shared_ptr<ClientSocket> client, std::string_view name, std::string_view avatar, int id, std::string_view uuid_str) {
  // create new Player and setup
  auto player = std::make_shared<Player>();
  // player->setSocket(client);
  // player->setParent(this);
  // client->disconnect(this);
  player->setScreenName(name.data());
  player->setAvatar(avatar.data());
  player->setId(id);
  player->setUuid(uuid_str.data());
  // if (players.count() <= 10) {
  //   broadcast("ServerMessage", tr("%1 logged in").arg(player->getScreenName()).toUtf8());
  // }

  addPlayer(player);

  setupPlayer(*player);

  // auto result = db->select(QString("SELECT totalGameTime FROM usergameinfo WHERE id=%1;").arg(id));
  // auto time = result[0]["totalGameTime"].toInt();
  // player->addTotalGameTime(time);
  // player->doNotify("AddTotalGameTime", QCborArray{ id, time }.toCborValue().toCbor());

  // lobby()->addPlayer(player);
}

void UserManager::setupPlayer(Player &player, bool all_info) {
  // tell the lobby player's basic property
  // QCborArray arr;
  // arr << player->getId();
  // arr << player->getScreenName();
  // arr << player->getAvatar();
  // arr << QDateTime::currentMSecsSinceEpoch();
  // player->doNotify("Setup", arr.toCborValue().toCbor());

  // if (all_info) {
  //   player->doNotify("SetServerSettings", QCborArray {
  //         getConfig("motd").toString(),
  //         QCborValue::fromJsonValue(getConfig("hiddenPacks")),
  //         getConfig("enableBots").toBool(),
  //         }.toCborValue().toCbor());
  // }
}

