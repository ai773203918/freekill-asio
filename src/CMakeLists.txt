# SPDX-License-Identifier: GPL-3.0-or-later

set(freekill_SRCS
  "main.cpp"

  # "core/player.cpp"
  # "core/util.cpp"
  # "core/c-wrapper.cpp"
  # "core/packman.cpp"

  "network/server_socket.cpp"
  "network/client_socket.cpp"
  # "network/router.cpp"

  # "server/server.cpp"

  # "server/user/auth.cpp"
  # "server/user/serverplayer.cpp"

  # "server/room/roombase.cpp"
  # "server/room/lobby.cpp"
  # "server/room/room.cpp"

  # "server/rpc-lua/jsonrpc.cpp"
  # "server/rpc-lua/rpc-lua.cpp"

  # "server/gamelogic/roomthread.cpp"
  # "server/gamelogic/rpc-dispatchers.cpp"

  # "server/cli/shell.cpp"
)

target_precompile_headers(freekill-asio PRIVATE pch.h)
target_sources(freekill-asio PRIVATE ${freekill_SRCS})
target_link_libraries(freekill-asio PRIVATE
  sqlite3
  OpenSSL::Crypto
  git2
  spdlog::spdlog
)
