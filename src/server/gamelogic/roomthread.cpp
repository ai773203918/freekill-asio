// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/gamelogic/roomthread.h"
#include "server/server.h"
// #include "core/util.h"
#include "core/c-wrapper.h"
// #include "server/rpc-lua/rpc-lua.h"
#include "server/gamelogic/rpc-dispatchers.h"
#include "server/user/player.h"
#include "server/user/user_manager.h"
#include "server/rpc-lua/rpc-lua.h"

#include <sys/eventfd.h>
#include <thread>
#include <unistd.h>

using namespace std::literals;

RoomThread::RoomThread(asio::io_context &main_ctx) : io_ctx {},
  m_thread {} // 调用start后才有效
{
  static int nextThreadId = 1000;
  m_id = nextThreadId++;

  auto &server = Server::instance();
  m_capacity = server.config().roomCountPerThread;
  md5 = server.getMd5();

  // 在run中创建，这样就能在接下来的exec中处理事件了
  // 这集可以直接在构造函数创了 Qt故事里面是为了绑定到新线程对应的eventLoop
  L = std::make_unique<RpcLua>(io_ctx);

  push_request_callback = [&](const std::string &msg) {
    L->call("HandleRequest", msg);
  };
  delay_callback = [&](int roomId, int ms) {
    auto timer = std::make_shared<asio::steady_timer>(io_ctx, std::chrono::milliseconds(ms));
    auto weak = weak_from_this();
    timer->async_wait([this, weak, roomId, timer](const asio::error_code& ec){
      if (!ec) {
        auto t = weak.lock();
        if (!t) return;
        this->L->call("ResumeRoom", roomId, "delay_done"sv);
      } else {
        spdlog::error("error in delay(): {}", ec.message());
      }
    });
  };
  wake_up_callback = [&](int roomId, const char *reason) {
    L->call("ResumeRoom", roomId, std::string_view { reason });
  };

  set_player_state_callback = [&](int connId, int roomId) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayerByConnId(connId).lock();
    if (!p) return;

    L->call("SetPlayerState", roomId, p->getId(), p->getState());
  };
  add_observer_callback = [&](int connId, int roomId) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayerByConnId(connId).lock();
    if (!p) return;

    L->call("AddObserver", roomId, RpcDispatchers::getPlayerObject(*p));
  };
  remove_observer_callback = [&](int connId, int roomId) {
    auto &um = Server::instance().user_manager();
    auto p = um.findPlayerByConnId(connId).lock();
    if (!p) return;

    L->call("RemoveObserver", roomId, p->getId());
  };

  start();
}

RoomThread::~RoomThread() {
  io_ctx.stop();
  m_thread.join();
  // spdlog::debug("[MEMORY] RoomThread {} destructed", m_id);
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
  if (fcntl(evt_fd, F_GETFD) == -1) return;
  uint64_t value = 1;
  ::write(evt_fd, &value, sizeof(value));
}

void RoomThread::emit_signal(std::function<void()> f) {
  if (!L->alive()) {
    md5 = ""; // outdated = true;
    return;
  }
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

const RpcLua &RoomThread::getLua() const {
  return *L;
}

bool RoomThread::isFull() const {
  return m_capacity <= m_ref_count;
}

int RoomThread::getCapacity() const { return m_capacity; }

std::string RoomThread::getMd5() const { return md5; }

bool RoomThread::isOutdated() {
  bool ret = md5 != Server::instance().getMd5();
  if (ret) {
    // 让以后每次都outdate
    // 不然反复disable/enable的情况下会出乱子
    md5 = "";
  }
  return ret;
}

int RoomThread::getRefCount() const {
  return m_ref_count;
}

void RoomThread::increaseRefCount() {
  m_ref_count++;
}

void RoomThread::decreaseRefCount() {
  m_ref_count--;
  if (m_ref_count > 0) return;

  if (isOutdated()) {
    asio::post(Server::instance().context(), [this](){
      Server::instance().removeThread(m_id);
    });
  }
}
