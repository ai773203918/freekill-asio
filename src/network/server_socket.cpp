// SPDX-License-Identifier: GPL-3.0-or-later

#include "network/server_socket.h"
#include "network/client_socket.h"

#include "server/server.h"
#include "server/user/user_manager.h"

#include <cjson/cJSON.h>

ServerSocket::ServerSocket(asio::io_context &io_ctx, tcp::endpoint end, udp::endpoint udpEnd):
  m_acceptor { io_ctx, end }, m_udp_socket { io_ctx, udpEnd }
{
  spdlog::info("server is ready to listen on {}", end.port());
}

void ServerSocket::listen() {
  m_acceptor.async_accept([this](const asio::error_code &err, tcp::socket socket) {
    if (!err) {
      auto conn = std::make_shared<ClientSocket>(std::move(socket));

      if (new_connection_callback) {
        new_connection_callback(conn);
        conn->start();
      }
    } else {
      spdlog::error("Accept error: {}", err.message());
    }

    listen();
  });
}

void ServerSocket::listen_udp() {
  m_udp_socket.async_receive_from(
    asio::buffer(udp_recv_buffer), udp_remote_end,
    [this](const asio::error_code &ec, size_t len) {
      auto sv = std::string_view { udp_recv_buffer.data(), len };
      // spdlog::debug("RX (udp [{}]:{}): {}", udp_remote_end.address().to_string(), udp_remote_end.port(), sv);
      if (sv == "fkDetectServer") {
        m_udp_socket.async_send_to(
          asio::const_buffer("me", 2),
          udp_remote_end,
          [](const asio::error_code &ec, size_t){});
      } else if (sv.starts_with("fkGetDetail,")) {
        auto &conf = Server::instance().config();
        auto &um = Server::instance().user_manager();

        cJSON *jsonArray = cJSON_CreateArray();
        cJSON_AddItemToArray(jsonArray, cJSON_CreateString("0.1-asio"));
        cJSON_AddItemToArray(jsonArray, cJSON_CreateString(conf.iconUrl.c_str()));
        cJSON_AddItemToArray(jsonArray, cJSON_CreateString(conf.description.c_str()));
        cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(conf.capacity));
        cJSON_AddItemToArray(jsonArray, cJSON_CreateNumber(um.getPlayers().size()));
        cJSON_AddItemToArray(jsonArray, cJSON_CreateString(std::string(sv).substr(12).c_str()));

        char *json = cJSON_PrintUnformatted(jsonArray);
        m_udp_socket.async_send_to(
          asio::const_buffer(json, strlen(json)),
          udp_remote_end,
          [](const asio::error_code &ec, size_t){});

        // spdlog::debug("TX (udp [{}]:{}): {}", udp_remote_end.address().to_string(), udp_remote_end.port(), json);
        cJSON_Delete(jsonArray);
        free(json);
      }

      listen_udp();
    }
  );
}

void ServerSocket::set_new_connection_callback(std::function<void(std::shared_ptr<ClientSocket>)> f) {
  new_connection_callback = f;
}

