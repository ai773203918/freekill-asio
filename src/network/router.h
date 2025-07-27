// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

struct Packet;
class Player;
class ClientSocket;

class Router {
public:
  enum PacketType {
    TYPE_REQUEST = 0x100,      ///< 类型为Request的包
    TYPE_REPLY = 0x200,        ///< 类型为Reply的包
    TYPE_NOTIFICATION = 0x400, ///< 类型为Notify的包
    SRC_CLIENT = 0x010,        ///< 从客户端发出的包
    SRC_SERVER = 0x020,        ///< 从服务端发出的包
    SRC_LOBBY = 0x040,
    DEST_CLIENT = 0x001,
    DEST_SERVER = 0x002,
    DEST_LOBBY = 0x004
  };

  enum RouterType {
    TYPE_SERVER,
    TYPE_CLIENT
  };

  Router() = delete;
  Router(Player *player, ClientSocket *socket, RouterType type);
  ~Router();

  ClientSocket *getSocket() const;
  void setSocket(ClientSocket *socket);
  void removeSocket();

  // signal connectors
  void set_message_ready_callback(std::function<void(const std::vector<uint8_t>&)> callback);
  void set_reply_ready_callback(std::function<void()> callback);
  void set_notification_got_callback(std::function<void(const Packet &)> callback);

  /*
  void setReplyReadySemaphore(QSemaphore *semaphore);

  void request(int type, const QByteArray &command,
              const QByteArray &cborData, int timeout, qint64 timestamp = -1);
  void reply(int type, const QByteArray &command, const QByteArray &cborData);
  */
  void notify(int type, const std::string_view &command, const std::string_view &cborData);

  /*
  int getTimeout() const;

  void cancelRequest();
  void abortRequest();

  QString waitForReply(int timeout);

  int getRequestId() const { return requestId; }
  qint64 getRequestTimestamp() { return requestTimestamp; }
  */

protected:
  void handlePacket(const Packet &packet);

private:
  ClientSocket *socket;
  Player *player = nullptr;

  RouterType type;

/*

  // For client side
  int requestId;
  int requestTimeout;
  qint64 requestTimestamp;

  // For server side
  QDateTime requestStartTime;
  QMutex replyMutex;
  int expectedReplyId;
  int replyTimeout;
  QString m_reply;    // should be json string
  QSemaphore replyReadySemaphore;
  QSemaphore *extraReplyReadySemaphore;

  void sendMessage(const QByteArray &msg);
  */

  // signals
  std::function<void(const std::vector<uint8_t>&)> message_ready_callback;
  std::function<void()> reply_ready_callback;
  std::function<void(const Packet &)> notification_got_callback;
};
