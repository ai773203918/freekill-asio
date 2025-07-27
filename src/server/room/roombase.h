#ifndef _ROOMBASE_H
#define _ROOMBASE_H

class Server;
class Player;

struct Packet;

class RoomBase {
public:
  bool isLobby() const;

  int getId() const;

  /*
  QList<Player *> getPlayers() const;
  QList<Player *> getOtherPlayers(Player *expect) const;
  Player *findPlayer(int id) const;

  void doBroadcastNotify(const QList<Player *> targets,
                         const QByteArray &command, const QByteArray &jsonData);

  void chat(Player *sender, const QString &jsonData);

  */

  virtual void addPlayer(Player &player) = 0;
  virtual void removePlayer(Player &player) = 0;
  virtual void handlePacket(Player &sender, const Packet &packet) = 0;

protected:
  int id;
};

#endif // _ROOMBASE_H
