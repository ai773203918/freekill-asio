// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

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

  Router(QObject *parent, ClientSocket *socket, RouterType type);
  ~Router();

  ClientSocket *getSocket() const;
  void setSocket(ClientSocket *socket);
  void removeSocket();
  bool isConsoleStart() const;

  void setReplyReadySemaphore(QSemaphore *semaphore);

  void request(int type, const QByteArray &command,
              const QByteArray &cborData, int timeout, qint64 timestamp = -1);
  void reply(int type, const QByteArray &command, const QByteArray &cborData);
  void notify(int type, const QByteArray &command, const QByteArray &cborData);

  int getTimeout() const;

  void cancelRequest();
  void abortRequest();

  QString waitForReply(int timeout);

  int getRequestId() const { return requestId; }
  qint64 getRequestTimestamp() { return requestTimestamp; }

signals:
  void messageReady(const QByteArray &msg);
  void replyReady();

  void notification_got(const QByteArray &command, const QByteArray &cborData);
  void request_got(const QByteArray &command, const QByteArray &cborData);

protected:
  void handlePacket(const QCborArray &packet);

private:
  std::unique_ptr<ClientSocket> socket;
  RouterType type;

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
};
