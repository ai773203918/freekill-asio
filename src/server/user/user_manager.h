#pragma once

class ClientSocket;
class Player;
class AuthManager;

class UserManager {
public:
  explicit UserManager();

  Player *findPlayer(int id) const;
  Player *findPlayerByConnId(int connId) const;
  void addPlayer(std::shared_ptr<Player> player);
  void deletePlayer(Player &p);
  void removePlayer(int id);
  void removePlayerByConnId(int connid);

  void processNewConnection(std::shared_ptr<ClientSocket> client);

  void createNewPlayer(std::shared_ptr<ClientSocket> client, std::string_view name, std::string_view avatar, int id, std::string_view uuid_str);
  Player &createRobot();

  void setupPlayer(Player &player, bool all_info = true);

private:
  std::unique_ptr<AuthManager> m_auth;

  // connId -> Player
  std::unordered_map<int, std::shared_ptr<Player>> players_map;
  // Id -> Player
  std::unordered_map<int, std::shared_ptr<Player>> robots_map;
  std::unordered_map<int, std::shared_ptr<Player>> online_players_map;

  Player *findRobot(int id) const;
};
