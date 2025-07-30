// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/packman.h"
#include "server/server.h"
#include "server/cli/shell.h"
#include "server/gamelogic/roomthread.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/ansicolor_sink.h>

static void initLogger() {
  auto logger_file = "freekill.server.log";

  std::vector<spdlog::sink_ptr> sinks;
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

  stdout_sink->set_color_mode(spdlog::color_mode::always);
  sinks.push_back(stdout_sink);

  // 单个log文件最大30M 最多备份5个 算上当前log文件的话最多同时存在6个log
  // 解决了牢版服务器关服后log消失的事情 伟大
  auto rotate_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logger_file, 1048576 * 30, 5);
  sinks.push_back(rotate_sink);

  auto spd_logger = std::make_shared<spdlog::logger>("fk-logger", begin(sinks), end(sinks));
  spdlog::register_logger(spd_logger);
  spdlog::set_default_logger(spd_logger);
  spdlog::set_pattern("[%C-%m-%d %H:%M:%S.%f] [%t/%^%L%$] %v");
  spdlog::set_level(spdlog::level::trace);
  spdlog::flush_every(std::chrono::seconds(3));

  // TODO 将Shell改为Server的成员后，添加callback处理此事
  // if (ShellInstance && !ShellInstance->lineDone()) {
  //   ShellInstance->redisplay();
  // }
}

int main(int argc, char *argv[]) {
  initLogger();

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
