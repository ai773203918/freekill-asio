// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/cli/shell.h"
#include "core/packman.h"
// #include "server/rpc-lua/rpc-lua.h"
#include "server/server.h"
#include "server/user/player.h"
#include "server/user/user_manager.h"
#include "server/room/room_manager.h"
#include "server/room/room.h"
#include "server/room/lobby.h"
#include "server/rpc-lua/rpc-lua.h"
#include "server/gamelogic/roomthread.h"
#include "core/util.h"
#include "core/c-wrapper.h"

#include <readline/history.h>
#include <readline/readline.h>
#include <signal.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <cstdio>
#include <cstring>

static constexpr const char *prompt = "fk-asio> ";

void Shell::helpCommand(StringList &) {
  spdlog::info("Frequently used commands:");
#define HELP_MSG(a, b)                                                         \
  spdlog::info((a), Color((b), fkShell::Cyan));

  HELP_MSG("{}: Display this help message.", "help");
  HELP_MSG("{}: Shut down the server.", "quit");
  HELP_MSG("{}: Crash the server. Useful when encounter dead loop.", "crash");
  HELP_MSG("{}: List all online players.", "lsplayer");
  HELP_MSG("{}: List all running rooms, or show player of room by an <id>.", "lsroom");
  HELP_MSG("{}: Reload server config file.", "reloadconf/r");
  HELP_MSG("{}: Kick a player by his <id>.", "kick");
  HELP_MSG("{}: Broadcast message.", "msg/m");
  HELP_MSG("{}: Broadcast message to a room.", "msgroom/mr");
  HELP_MSG("{}: Ban 1 or more accounts, IP, UUID by their <name>.", "ban");
  HELP_MSG("{}: Unban 1 or more accounts by their <name>.", "unban");
  HELP_MSG(
      "{}: Ban 1 or more IP address. "
      "At least 1 <name> required.",
      "banip");
  HELP_MSG(
      "{}: Unban 1 or more IP address. "
      "At least 1 <name> required.",
      "unbanip");
  HELP_MSG(
      "{}: Ban 1 or more UUID. "
      "At least 1 <name> required.",
      "banuuid");
  HELP_MSG(
      "{}: Unban 1 or more UUID. "
      "At least 1 <name> required.",
      "unbanuuid");
  HELP_MSG("{}: reset <name>'s password to 1234.", "resetpassword/rp");
  HELP_MSG("{}: View status of server.", "stat/gc");
  HELP_MSG("{}: View detail information (Lua) of room by an id.", "dumproom");
  HELP_MSG("{}: Kick all players in a room, then abandon it.", "killroom");
  spdlog::info("");
  spdlog::info("===== Package commands =====");
  HELP_MSG("{}: Install a new package from <url>.", "install");
  HELP_MSG("{}: Remove a package.", "remove");
  HELP_MSG("{}: List all packages.", "pkgs");
  HELP_MSG("{}: Get packages hash from file system and write to database.", "syncpkgs");
  HELP_MSG("{}: Enable a package.", "enable");
  HELP_MSG("{}: Disable a package.", "disable");
  HELP_MSG("{}: Upgrade a package. Leave empty to upgrade all.", "upgrade/u");
  spdlog::info("For more commands, check the documentation.");

#undef HELP_MSG
}

Shell::~Shell() {
  rl_clear_history();
  m_thread.join();
}

void Shell::start() {
  m_thread = std::thread(&Shell::run, this);
}

void Shell::lspCommand(StringList &) {
  auto &user_manager = Server::instance().user_manager();
  auto &players = user_manager.getPlayers();
  if (players.size() == 0) {
    spdlog::info("No online player.");
    return;
  }
  spdlog::info("Current {} online player(s) are:", players.size());
  for (auto &[_, player] : players) {
    spdlog::info("{}, {}", player->getId(), player->getScreenName());
  }
}

void Shell::lsrCommand(StringList &list) {
  auto &user_manager = Server::instance().user_manager();
  auto &room_manager = Server::instance().room_manager();
  if (!list.empty() && !list[0].empty()) {
    auto pid = list[0];
    int id = std::atoi(pid.c_str());

    auto room = room_manager.findRoom(id);
    if (!room) {
      if (id != 0) {
        spdlog::info("No such room.");
      } else {
        spdlog::info("You are viewing lobby, players in lobby are:");

        auto &lobby = room_manager.lobby();
        for (auto &[pid, _] : lobby.getPlayers()) {
          auto p = user_manager.findPlayerByConnId(pid);
          if (p) spdlog::info("{}, {}", p->getId(), p->getScreenName());
        }
      }
    } else {
      auto pw = room->getPassword();
      spdlog::info("{}, {} [pw={}]", room->getId(), room->getName(),
        pw == "" ? "<nil>" : pw);
      spdlog::info("Players in this room:");

      for (auto pid : room->getPlayers()) {
        auto p = user_manager.findPlayerByConnId(pid);
        if (p) spdlog::info("{}, {}", p->getId(), p->getScreenName());
      }
    }

    return;
  }

  const auto &rooms = room_manager.getRooms();
  if (rooms.size() == 0) {
    spdlog::info("No running room.");
    return;
  }
  spdlog::info("Current {} running rooms are:", rooms.size());
  for (auto &[_, room] : rooms) {
    auto pw = room->getPassword();
    spdlog::info("{}, {} [pw={}]", room->getId(), room->getName(),
                 pw == "" ? "<nil>" : pw);
  }
}

void Shell::installCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'install' command need a URL to install.");
    return;
  }

  auto url = list[0];
  PackMan::instance().downloadNewPack(url.c_str());
  Server::instance().refreshMd5();
}

void Shell::removeCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'remove' command need a package name to remove.");
    return;
  }

  auto pack = list[0];
  PackMan::instance().removePack(pack.c_str());
  Server::instance().refreshMd5();
}

void Shell::upgradeCommand(StringList &list) {
  if (list.empty()) {
    auto arr = PackMan::instance().listPackages();
    for (auto &a : arr) {
      PackMan::instance().upgradePack(a["name"].c_str());
    }
    Server::instance().refreshMd5();
    return;
  }

  auto pack = list[0];
  PackMan::instance().upgradePack(pack.c_str());
  Server::instance().refreshMd5();
}

void Shell::enableCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'enable' command need a package name to enable.");
    return;
  }

  auto pack = list[0];
  PackMan::instance().enablePack(pack.c_str());
  Server::instance().refreshMd5();
}

void Shell::disableCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'disable' command need a package name to disable.");
    return;
  }

  auto pack = list[0];
  PackMan::instance().disablePack(pack.c_str());
  Server::instance().refreshMd5();
}

void Shell::lspkgCommand(StringList &) {
  auto arr = PackMan::instance().listPackages();
  spdlog::info("Name\tVersion\t\tEnabled");
  spdlog::info("------------------------------");
  for (auto &a : arr) {
    auto hash = a["hash"];
    spdlog::info("{}\t{}\t{}", a["name"], hash.substr(0, 8), a["enabled"]);
  }
}

void Shell::syncpkgCommand(StringList &) {
  PackMan::instance().syncCommitHashToDatabase();
  spdlog::info("Done.");
}

void Shell::kickCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'kick' command needs a player id.");
    return;
  }

  auto pid = list[0];
  int id = std::stoi(pid);

  auto p = Server::instance().user_manager().findPlayer(id);
  if (p) {
    p->emitKicked();
  }
}

void Shell::msgCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'msg' command needs message body.");
    return;
  }

  std::string msg;
  for (auto &s : list) {
    msg += s;
    msg += ' ';
  }
  Server::instance().broadcast("ServerMessage", msg);
}

void Shell::msgRoomCommand(StringList &list) {
  if (list.size() < 2) {
    spdlog::warn("The 'msgroom' command needs <roomId> and message body.");
    return;
  }

  auto roomId = stoi(list[0]);
  auto room = Server::instance().room_manager().findRoom(roomId);
  if (!room) {
    spdlog::info("No such room.");
    return;
  }
  std::string msg;
  for (int i = 1; i < list.size(); i++) {
    msg += list[i];
    msg += ' ';
  }
  room->doBroadcastNotify(room->getPlayers(), "ServerMessage", msg);
}

static void banAccount(Sqlite3 &db, const std::string_view &name, bool banned) {
  if (!Sqlite3::checkString(name))
    return;
  static constexpr const char *sql_find =
    "SELECT id FROM userinfo WHERE name='{}';";
  auto result = db.select(fmt::format(sql_find, name));
  if (result.empty())
    return;
  auto obj = result[0];
  int id = stoi(obj["id"]);
  db.exec(fmt::format("UPDATE userinfo SET banned={} WHERE id={};",
                  banned ? 1 : 0, id));

  if (banned) {
    auto p = Server::instance().user_manager().findPlayer(id);
    if (p) {
      p->emitKicked();
    }
    spdlog::info("Banned {}.", name);
  } else {
    spdlog::info("Unbanned {}.", name);
  }
}

void Shell::banCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'ban' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();

  for (auto &name : list) {
    banAccount(db, name, true);
  }
}

void Shell::unbanCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'unban' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();

  for (auto &name : list) {
    banAccount(db, name, false);
  }

  // unbanipCommand(list);
  unbanUuidCommand(list);
}

static void banIPByName(Sqlite3 &db, const std::string_view &name, bool banned) {
  if (!Sqlite3::checkString(name))
    return;

  static constexpr const char *sql_find =
    "SELECT id, lastLoginIp FROM userinfo WHERE name='{}';";
  auto result = db.select(fmt::format(sql_find, name));
  if (result.empty())
    return;
  auto obj = result[0];
  int id = stoi(obj["id"]);
  auto addr = obj["lastLoginIp"];

  if (banned) {
    db.exec(fmt::format("INSERT INTO banip VALUES('{}');", addr));

    auto p = Server::instance().user_manager().findPlayer(id);
    if (p) {
      p->emitKicked();
    }
    spdlog::info("Banned IP {}.", addr);
  } else {
    db.exec(fmt::format("DELETE FROM banip WHERE ip='{}';", addr));
    spdlog::info("Unbanned IP {}.", addr);
  }
}

void Shell::banipCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'banip' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();

  for (auto &name : list) {
    banIPByName(db, name, true);
  }
}

void Shell::unbanipCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'unbanip' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();

  for (auto &name : list) {
    banIPByName(db, name, false);
  }
}

static void banUuidByName(Sqlite3 &db, const std::string_view &name, bool banned) {
  if (!Sqlite3::checkString(name))
    return;
  static constexpr const char *sql_find =
    "SELECT id FROM userinfo WHERE name='{}';";
  auto result = db.select(fmt::format(sql_find, name));
  if (result.empty())
    return;
  auto obj = result[0];
  int id = stoi(obj["id"]);

  auto result2 = db.select(fmt::format("SELECT * FROM uuidinfo WHERE id={};", id));
  if (result2.empty())
    return;

  auto uuid = result2[0]["uuid"];

  if (banned) {
    db.exec(fmt::format("INSERT INTO banuuid VALUES('{}');", uuid));

    auto p = Server::instance().user_manager().findPlayer(id);
    if (p) {
      p->emitKicked();
    }
    spdlog::info("Banned UUID {}.", uuid);
  } else {
    db.exec(fmt::format("DELETE FROM banuuid WHERE uuid='{}';", uuid));
    spdlog::info("Unbanned UUID {}.", uuid);
  }
}

void Shell::banUuidCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'banuuid' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();

  for (auto &name : list) {
    banUuidByName(db, name, true);
  }
}

void Shell::unbanUuidCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'unbanuuid' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();

  for (auto &name : list) {
    banUuidByName(db, name, false);
  }
}

void Shell::reloadConfCommand(StringList &) {
  Server::instance().reloadConfig();
  spdlog::info("Reloaded server config file.");
}

void Shell::resetPasswordCommand(StringList &list) {
  if (list.empty()) {
    spdlog::warn("The 'resetpassword' command needs at least 1 <name>.");
    return;
  }

  auto &db = Server::instance().database();
  for (auto &name : list) {
    // 重置为1234
    db.exec(fmt::format("UPDATE userinfo SET password="
          "'dbdc2ad3d9625407f55674a00b58904242545bfafedac67485ac398508403ade',"
          "salt='00000000' WHERE name='{}';", name));
  }
}

/*
static QString formatMsDuration(qint64 time) {
  QString ret;

  auto ms = time % 1000;
  time /= 1000;
  auto sec = time % 60;
  ret = QString("{}.{} seconds").arg(sec).arg(ms) + ret;
  time /= 60;
  if (time == 0) return ret;

  auto min = time % 60;
  ret = QString("{} minutes, ").arg(min) + ret;
  time /= 60;
  if (time == 0) return ret;

  auto hour = time % 24;
  ret = QString("{} hours, ").arg(hour) + ret;
  time /= 24;
  if (time == 0) return ret;

  ret = QString("{} days, ").arg(time) + ret;
  return ret;
}
*/

void Shell::statCommand(StringList &) {
  auto &server = Server::instance();
  // auto uptime_ms = server->getUptime();
  // spdlog::info("uptime: {}", formatMsDuration(uptime_ms).toUtf8().constData());

  auto players = server.user_manager().getPlayers();
  spdlog::info("Player(s) logged in: {}", players.size());
  // spdlog::info("Rooms: {}", server.room_manager().getRooms().size());

  auto &threads = server.getThreads();
  for (auto &[id, thr] : threads) {
    // auto rooms = thr->findChildren<Room *>();
    auto &L = thr->getLua();

    auto stat_str = L.getConnectionInfo();
    auto outdated = thr->isOutdated();
    // if (rooms.count() == 0 && outdated) {
      // thr->deleteLater();
    // } else {
      spdlog::info("RoomThread {} | {} | {} room(s) {}", id, stat_str, '?', //rooms.count(),
            outdated ? "| Outdated" : "");
    // }
  }

  spdlog::info("Database memory usage: {:.2f} MiB",
        ((double)server.database().getMemUsage()) / 1048576);
}

void Shell::killRoomCommand(StringList &list) {
  if (list.empty() || list[0].empty()) {
    spdlog::warn("Need room id to do this.");
    return;
  }

  auto pid = list[0];
  int id = stoi(pid);

  auto &um = Server::instance().user_manager();
  auto &rm = Server::instance().room_manager();
  auto room = rm.findRoom(id);
  if (!room) {
    spdlog::info("No such room.");
  } else {
    spdlog::info("Killing room {}", id);

    for (auto pConnId : room->getPlayers()) {
      auto player = um.findPlayerByConnId(pConnId);
      if (player && player->getId() > 0)
        player->emitKicked();
    }
    room->checkAbandoned();
  }
}

static void sigintHandler(int) {
  rl_reset_line_state();
  rl_replace_line("", 0);
  rl_crlf();
  rl_forced_update_display();
}
static char **fk_completion(const char *text, int start, int end);
static char *null_completion(const char *, int) { return NULL; }

Shell::Shell() {
  // setObjectName("Shell");
  // Setup readline here

  // 别管Ctrl+C了
  //rl_catch_signals = 1;
  //rl_catch_sigwinch = 1;
  //rl_persistent_signal_handlers = 1;
  //rl_set_signals();
  signal(SIGINT, sigintHandler);
  rl_attempted_completion_function = fk_completion;
  rl_completion_entry_function = null_completion;

  static const std::unordered_map<std::string_view, void (Shell::*)(StringList &)> handlers = {
    {"help", &Shell::helpCommand},
    {"?", &Shell::helpCommand},
    {"lsplayer", &Shell::lspCommand},
    {"lsroom", &Shell::lsrCommand},
    {"install", &Shell::installCommand},
    {"remove", &Shell::removeCommand},
    {"upgrade", &Shell::upgradeCommand},
    {"u", &Shell::upgradeCommand},
    {"pkgs", &Shell::lspkgCommand},
    {"syncpkgs", &Shell::syncpkgCommand},
    {"enable", &Shell::enableCommand},
    {"disable", &Shell::disableCommand},
    {"kick", &Shell::kickCommand},
    {"msg", &Shell::msgCommand},
    {"m", &Shell::msgCommand},
    {"msgroom", &Shell::msgRoomCommand},
    {"mr", &Shell::msgRoomCommand},
    {"ban", &Shell::banCommand},
    {"unban", &Shell::unbanCommand},
    {"banip", &Shell::banipCommand},
    {"unbanip", &Shell::unbanipCommand},
    {"banuuid", &Shell::banUuidCommand},
    {"unbanuuid", &Shell::unbanUuidCommand},
    {"reloadconf", &Shell::reloadConfCommand},
    {"r", &Shell::reloadConfCommand},
    {"resetpassword", &Shell::resetPasswordCommand},
    {"rp", &Shell::resetPasswordCommand},
    {"stat", &Shell::statCommand},
    {"gc", &Shell::statCommand},
    {"killroom", &Shell::killRoomCommand},
    // special command
    {"quit", &Shell::helpCommand},
    {"crash", &Shell::helpCommand},
  };
  handler_map = handlers;
}

void Shell::handleLine(char *bytes) {
  if (!bytes || !strncmp(bytes, "quit", 4)) {
    spdlog::info("Server is shutting down.");
    Server::instance().stop();
    done = true;
    return;
  }

  spdlog::info("Running command: '{}'", bytes);

  if (!strncmp(bytes, "crash", 5)) {
    spdlog::error("Crashing."); // should dump core
    std::exit(1);
    return;
  }

  add_history(bytes);

  auto command = std::string { bytes };
  std::istringstream iss(command);
  std::vector<std::string> command_list;

  for (std::string token; iss >> token;) {
    command_list.push_back(token);
  }
  if (command_list.size() == 0) return;

  auto it = handler_map.find(command_list[0]);
  if (it == handler_map.end()) {
    auto bytes = command_list[0];
    spdlog::warn("Unknown command '{}'. Type 'help' for hints.", bytes);
  } else {
    command_list.erase(command_list.begin());
    (this->*it->second)(command_list);
  }

  free(bytes);
}

void Shell::redisplay() {
  // QString tmp = syntaxHighlight(rl_line_buffer);
  rl_clear_visible_line();
  rl_forced_update_display();

  //moveCursorToStart();
  //printf("\r{}{}", prompt, tmp.toUtf8().constData());
}

void Shell::moveCursorToStart() {
  winsize sz;
  ioctl(STDOUT_FILENO, TIOCGWINSZ, &sz);
  int lines = (rl_end + strlen(prompt) - 1) / sz.ws_col;
  printf("\e[%d;%dH", sz.ws_row - lines, 0);
}

void Shell::clearLine() {
  rl_clear_visible_line();
}

bool Shell::lineDone() const {
  return (bool)rl_done;
}

/*
// 最简单的语法高亮，若命令可执行就涂绿，否则涂红
QString Shell::syntaxHighlight(char *bytes) {
  QString ret(bytes);
  auto command = ret.split(' ').first();
  auto func = handler_map[command];
  auto colored_command = command;
  if (!func) {
    colored_command = Color(command, fkShell::Red, fkShell::Bold);
  } else {
    colored_command = Color(command, fkShell::Green);
  }
  ret.replace(0, command.length(), colored_command);
  return ret;
}
*/

char *Shell::generateCommand(const char *text, int state) {
  static int list_index, len;
  static std::vector<std::string_view> keys;
  static std::once_flag flag;
  std::call_once(flag, [&](){
    for (const auto &[k, _] : handler_map) {
      keys.push_back(k);
    }
  });
  const char *name;

  if (state == 0) {
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < keys.size()) {
    name = keys[list_index].data();
    ++list_index;
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return NULL;
}

static char *command_generator(const char *text, int state) {
  return Server::instance().shell().generateCommand(text, state);
}

static char *repo_generator(const char *text, int state) {
  static constexpr const char *recommend_repos[] = {
    "https://gitee.com/Qsgs-Fans/standard_ex",
    "https://gitee.com/Qsgs-Fans/shzl",
    "https://gitee.com/Qsgs-Fans/sp",
    "https://gitee.com/Qsgs-Fans/yj",
    "https://gitee.com/Qsgs-Fans/ol",
    "https://gitee.com/Qsgs-Fans/mougong",
    "https://gitee.com/Qsgs-Fans/mobile",
    "https://gitee.com/Qsgs-Fans/tenyear",
    "https://gitee.com/Qsgs-Fans/overseas",
    "https://gitee.com/Qsgs-Fans/jsrg",
    "https://gitee.com/Qsgs-Fans/qsgs",
    "https://gitee.com/Qsgs-Fans/mini",
    "https://gitee.com/Qsgs-Fans/gamemode",
    "https://gitee.com/Qsgs-Fans/utility",
    "https://gitee.com/Qsgs-Fans/freekill-core",
    "https://gitee.com/Qsgs-Fans/offline",
    "https://gitee.com/Qsgs-Fans/hegemony",
    "https://gitee.com/Qsgs-Fans/lunar",
  };
  static size_t list_index, len;
  const char *name;

  if (state == 0) {
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < std::size(recommend_repos)) {
    name = recommend_repos[list_index];
    ++list_index;
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }

  return NULL;
}

static char *package_generator(const char *text, int state) {
  /*
  static QJsonArray arr;
  static int list_index, len;
  const char *name;

  if (state == 0) {
    arr = QJsonDocument::fromJson(PackMan::instance().listPackages().toUtf8()).array();
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < arr.count()) {
    name = arr[list_index].toObject().value("name").toString().toUtf8().constData();
    ++list_index;
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }
  */

  return NULL;
}

static char *user_generator(const char *text, int state) {
  /*
  // TODO: userinfo表需要一个cache机制
  static Sqlite3::QueryResult arr;
  static int list_index, len;
  const char *name;

  if (state == 0) {
    arr = Server::instance().database()->select("SELECT name FROM userinfo;");
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < arr.count()) {
    name = arr[list_index]["name"].toUtf8().constData();
    ++list_index;
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }
  */

  return NULL;
};

static char *banned_user_generator(const char *text, int state) {
  /*
  // TODO: userinfo表需要一个cache机制
  static Sqlite3::QueryResult arr;
  static int list_index, len;
  const char *name;
  auto db = Server::instance().database();

  if (state == 0) {
    arr = db->select("SELECT name FROM userinfo WHERE banned = 1;");
    list_index = 0;
    len = strlen(text);
  }

  while (list_index < arr.count()) {
    name = arr[list_index]["name"].toUtf8().constData();
    ++list_index;
    if (strncmp(name, text, len) == 0) {
      return strdup(name);
    }
  }
  */

  return NULL;
};

static char **fk_completion(const char* text, int start, int end) {
  char **matches = NULL;
  if (start == 0) {
    matches = rl_completion_matches(text, command_generator);
  } else {
    auto str = std::string { rl_line_buffer };
    std::istringstream iss(str);
    std::vector<std::string> command_list;

    for (std::string token; iss >> token;) {
      command_list.push_back(token);
    }

    if (command_list.size() > 2) return NULL;
    auto command = command_list[0];
    if (command == "install") {
      matches = rl_completion_matches(text, repo_generator);
    } else if (command == "remove" || command == "upgrade"
        || command == "enable" || command == "disable") {
      matches = rl_completion_matches(text, package_generator);
    } else if (command.starts_with("ban") || command == "resetpassword" || command == "rp") {
      matches = rl_completion_matches(text, user_generator);
    } else if (command.starts_with("unban")) {
      matches = rl_completion_matches(text, banned_user_generator);
    }
  }

  return matches;
}

void Shell::run() {
  printf("\rfreekill-asio, Copyright (C) 2025, GNU GPL'd, by Notify et al.\n");
  printf("This program comes with ABSOLUTELY NO WARRANTY.\n");
  printf(
      "This is free software, and you are welcome to redistribute it under\n");
  printf("certain conditions; For more information visit "
         "http://www.gnu.org/licenses.\n\n");

  printf("[freekill-asio v0.0.1] Welcome to CLI. Enter 'help' for usage hints.\n");

  while (true) {
    char *bytes = readline(prompt);
    handleLine(bytes);
    if (done) break;
  }
}
