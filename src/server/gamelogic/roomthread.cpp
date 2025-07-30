// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/gamelogic/roomthread.h"
#include "server/server.h"
// #include "core/util.h"
#include "core/c-wrapper.h"
// #include "server/rpc-lua/rpc-lua.h"
// #include "server/gamelogic/rpc-dispatchers.h"
#include "server/user/player.h"
#include "server/user/user_manager.h"
#include "server/rpc-lua/rpc-lua.h"

#include <sys/eventfd.h>
#include <unistd.h>

using namespace std::literals;

/*
void Scheduler::addObserver(const QString &connId, int roomId) {
  auto p = ServerInstance->findPlayerByConnId(connId);
  if (!p) return;

  QVariantList gameData;
  for (auto i : p->getGameData()) gameData << i;

  L->call("AddObserver", {
    roomId,
    QVariantMap {
      { "connId", p->getConnId() },
      { "id", p->getId() },
      { "screenName", p->getScreenName() },
      { "avatar", p->getAvatar() },
      { "totalGameTime", p->getTotalGameTime() },

      { "state", p->getState() },

      { "gameData", gameData },
    }
  });
}
*/

RoomThread::RoomThread(asio::io_context &main_ctx) : io_ctx {}, main_io_ctx { main_ctx },
  m_thread {}, timer { io_ctx } // 调用start后才有效
{
  static int nextThreadId = 1000;
  m_id = nextThreadId++;
  // m_capacity = server->getConfig("roomCountPerThread").toInt(200);
  // md5 = server->getMd5();

  // 在run中创建，这样就能在接下来的exec中处理事件了
  // 这集可以直接在构造函数创了 Qt故事里面是为了绑定到新线程对应的eventLoop
  L = std::make_unique<RpcLua>(io_ctx);

  push_request_callback = [&](const std::string &msg) {
    L->call("HandleRequest", msg);
  };
  delay_callback = [&](int roomId, int ms) {
    spdlog::debug("delay {} ms", ms);
    timer = asio::steady_timer { io_ctx, std::chrono::milliseconds(ms) };
    timer.async_wait([&, roomId](const asio::error_code& ec){
      if (!ec) {
        L->call("ResumeRoom", roomId, "delay_done"sv);
      } else {
        spdlog::error(ec.message());
      }
    });
  };
  wake_up_callback = [&](int roomId, const char *reason) {
    spdlog::debug("ResumeRoom {} {}", roomId, std::string_view { reason });
    L->call("ResumeRoom", roomId, std::string_view { reason });
  };

  set_player_state_callback = [&](int connId, int roomId) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayerByConnId(connId);
    if (!p) return;

    L->call("SetPlayerState", roomId, p->getId(), p->getState());
  };
  add_observer_callback = [&](int connId, int roomId) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayerByConnId(connId);
    if (!p) return;
    // TODO
  };
  remove_observer_callback = [&](int connId, int roomId) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayerByConnId(connId);
    if (!p) return;

    L->call("RemoveObserver", roomId, p->getId());
  };

  start();
}

RoomThread::~RoomThread() {
  io_ctx.stop();
  m_thread.join();
}

int RoomThread::id() const {
  return m_id;
}

asio::io_context &RoomThread::context() {
  return io_ctx;
}

void RoomThread::start() {
  evt_fd = ::eventfd(0, 0);
  m_thread = std::thread([&](){
    // 直到调用quit()写evt_fd之前都让他一直等下去
    asio::posix::stream_descriptor eventfd_desc(io_ctx, evt_fd);
    char buf[16];
    eventfd_desc.async_read_some(
      asio::buffer(buf, 16),
      [&](asio::error_code err, std::size_t length) {
        if (!err) {
          ::close(evt_fd);
          spdlog::info("quit() called, eventfd is closed");
        }
      });

    io_ctx.run();
  });
}

void RoomThread::quit() {
  uint64_t value = 1;
  ::write(evt_fd, &value, sizeof(value));
}

void RoomThread::emit_signal(std::function<void()> f) {
  if (std::this_thread::get_id() == m_thread.get_id()) {
    f();
  } else {
    asio::post(io_ctx, f);
  }
}

void RoomThread::pushRequest(const std::string &req) {
  emit_signal(std::bind(push_request_callback, req));
}

void RoomThread::delay(int roomId, int ms) {
  emit_signal(std::bind(delay_callback, roomId, ms));
}

void RoomThread::wakeUp(int roomId, const char *reason) {
  emit_signal(std::bind(wake_up_callback, roomId, reason));
}

void RoomThread::setPlayerState(int connId, int roomId) {
  emit_signal(std::bind(set_player_state_callback, connId, roomId));
}

void RoomThread::addObserver(int connId, int roomId) {
  emit_signal(std::bind(add_observer_callback, connId, roomId));
}

void RoomThread::removeObserver(int connId, int roomId) {
  emit_signal(std::bind(remove_observer_callback, connId, roomId));
}


/*
bool RoomThread::isFull() const {
  return m_capacity <= findChildren<Room *>().length();
}

QString RoomThread::getMd5() const { return md5; }

Room *RoomThread::getRoom(int id) const {
  return m_server->findRoom(id);
}

bool RoomThread::isOutdated() {
  bool ret = md5 != m_server->getMd5();
  if (ret) {
    // 让以后每次都outdate
    // 不然反复disable/enable的情况下会出乱子
    md5 = QStringLiteral("");
  }
  return ret;
}

LuaInterface *RoomThread::getLua() const {
  return m_scheduler->getLua();
}

void RoomThread::onRoomAbandoned() {
  auto room = qobject_cast<Room *>(sender());

  if (room->getRefCount() == 0) {
    room->deleteLater();
  } else {
    wakeUp(room->getId(), "abandon");
  }
}
*/
