#include "server/user/user_manager.h"
#include "server/user/player.h"
#include "server/user/auth.h"
#include "server/server.h"
#include "server/room/room_manager.h"
#include "server/room/lobby.h"
#include "network/client_socket.h"
#include "network/router.h"
#include "core/c-wrapper.h"

UserManager::UserManager() {
  m_auth = std::make_unique<AuthManager>();
}

Player *UserManager::findPlayer(int id) const {
  auto it = online_players_map.find(id);
  if (it != online_players_map.end()) {
    return it->second.get();
  }
  return nullptr;
}

Player *UserManager::findPlayerByConnId(const std::string_view connId) const {
  auto it = players_map.find(connId);
  if (it != players_map.end()) {
    return it->second.get();
  }
  return nullptr;
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
  client->set_message_got_callback(
    std::bind(&AuthManager::processNewConnection, m_auth.get(), client, std::placeholders::_1));
  // client->timerSignup.start(30000);
}

void UserManager::createNewPlayer(std::shared_ptr<ClientSocket> client, std::string_view name, std::string_view avatar, int id, std::string_view uuid_str) {
  // create new Player and setup
  auto player = std::make_shared<Player>();
  player->router().setSocket(client.get());
  // player->setParent(this);
  // client->disconnect(this);
  player->setState(Player::Online);
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

  auto lobby = Server::instance().room_manager().lobby();
  lobby.addPlayer(*player);
}

void UserManager::setupPlayer(Player &player, bool all_info) {
  // tell the lobby player's basic property
  player.doNotify("Setup", Cbor::encodeArray({
    player.getId(),
    player.getScreenName().data(),
    player.getAvatar().data(),
    std::chrono::duration_cast<std::chrono::milliseconds>(
      std::chrono::system_clock::now().time_since_epoch()).count(),
  }));

  // if (all_info) {
  //   player->doNotify("SetServerSettings", QCborArray {
  //         getConfig("motd").toString(),
  //         QCborValue::fromJsonValue(getConfig("hiddenPacks")),
  //         getConfig("enableBots").toBool(),
  //         }.toCborValue().toCbor());
  // }
}

