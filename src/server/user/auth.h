#ifndef _AUTH_H
#define _AUTH_H

class Server;
class Sqlite3;
class ClientSocket;
class AuthManagerPrivate;

class AuthManager {
public:
  AuthManager();
  ~AuthManager() noexcept;
  // auto getPublicKey() const { return public_key; }

  void processNewConnection(std::shared_ptr<ClientSocket> conn);

private:
  /*
  Sqlite3 *db;
  QString public_key;
  AuthManagerPrivate *p_ptr;

  bool loadSetupData(const QCborArray &msg);
  bool checkVersion();
  bool checkIfUuidNotBanned();
  bool checkMd5();
  QMap<QString, QString> checkPassword();
  QMap<QString, QString> queryUserInfo(const QByteArray &decrypted_pw);

  void updateUserLoginData(int id);
  */
};

#endif // _AUTH_H
