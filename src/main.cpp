// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/packman.h"
#include "server/server.h"
#include "server/cli/shell.h"
#include "server/gamelogic/roomthread.h"

int main(int argc, char *argv[]) {
  spdlog::set_level(spdlog::level::trace);

  Pacman = new PackMan; // TODO 别用new 改成Server::instance那样

  asio::io_context io_ctx;

  {
    // 限定引用生命周期
    auto &server = Server::instance();
    server.listen( io_ctx, tcp::endpoint(tcp::v6(), 9527u) );

    auto thr = server.getThread(0);
    if (thr) {
      // thr->quit();
    }
  }

  // TODO 改为Server的子成员并unique_ptr
  Shell shell;
  shell.start();

  io_ctx.run();

  // 以下是main收尾部分的clear环节

  // 手动释放确保一切都在控制下
  Server::destroy();
  delete Pacman; // TODO

  spdlog::shutdown();
}
