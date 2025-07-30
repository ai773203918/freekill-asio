// SPDX-License-Identifier: GPL-3.0-or-later

#include "core/packman.h"
#include "server/server.h"
#include "server/cli/shell.h"
#include "server/gamelogic/roomthread.h"

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/callback_sink.h>

// 也是一种很蠢的方式了
static bool shellAlive = false;

static void initLogger() {
  auto logger_file = "freekill.server.log";

  std::vector<spdlog::sink_ptr> sinks;
  auto stdout_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();

  stdout_sink->set_color_mode(spdlog::color_mode::always);
  stdout_sink->set_pattern("\r[%C-%m-%d %H:%M:%S.%f] [%t/%^%L%$] %v");
  sinks.push_back(stdout_sink);

  // 单个log文件最大30M 最多备份5个 算上当前log文件的话最多同时存在6个log
  // 解决了牢版服务器关服后log消失的事情 伟大
  auto rotate_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(logger_file, 1048576 * 30, 5);
  rotate_sink->set_pattern("[%C-%m-%d %H:%M:%S.%f] [%t/%L] %v");
  sinks.push_back(rotate_sink);

  auto callback_sink = std::make_shared<spdlog::sinks::callback_sink_mt>([](const spdlog::details::log_msg &) {
    if (!shellAlive) return;
    auto &shell = Server::instance().shell();
    if (!shell.lineDone()) shell.redisplay();
  });
  sinks.push_back(callback_sink);

  auto spd_logger = std::make_shared<spdlog::logger>("fk-logger", begin(sinks), end(sinks));
  spdlog::register_logger(spd_logger);
  spdlog::set_default_logger(spd_logger);
  // spdlog::set_pattern("\r[%C-%m-%d %H:%M:%S.%f] [%t/%^%L%$] %v");
  spdlog::set_level(spdlog::level::trace);
  spdlog::flush_every(std::chrono::seconds(3));
}

int main(int argc, char *argv[]) {
  initLogger();

  asio::io_context io_ctx;
  Server::instance().listen( io_ctx, tcp::endpoint(tcp::v6(), 9527u) );

  shellAlive = true;

  io_ctx.run();

  shellAlive = false;

  // 手动释放确保一切都在控制下
  Server::destroy();
  PackMan::destroy();

  spdlog::shutdown();
}
