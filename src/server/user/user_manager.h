#pragma once

class ClientSocket;
class Player;

class UserManager {
public:
  Player *findPlayer(int id) const;
  Player *findPlayerByConnId(const std::string_view connId) const;
  void addPlayer(std::shared_ptr<Player> player);
  void removePlayer(int id);
  void removePlayerByConnId(const std::string_view connid);

  void processNewConnection(std::shared_ptr<ClientSocket> client);

  void createNewPlayer(std::shared_ptr<ClientSocket> client, std::string_view name, std::string_view avatar, int id, std::string_view uuid_str);

  void setupPlayer(Player *player, bool all_info = true);

private:
  // connId -> Player &
  std::unordered_map<std::string_view, std::shared_ptr<Player>> players_map;
  std::unordered_map<int, std::shared_ptr<Player>> online_players_map;
};
