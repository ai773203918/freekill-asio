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
  std::string_view getPublicKeyCbor() const;

  void processNewConnection(std::shared_ptr<ClientSocket> conn);

private:
  unsigned char *public_key_cbor_buf;
  size_t public_key_cbor_bufsize;
  std::unique_ptr<AuthManagerPrivate> p_ptr;
  /*
  Sqlite3 *db;

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
