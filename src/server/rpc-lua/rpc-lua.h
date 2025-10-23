// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "server/rpc-lua/jsonrpc.h"

class RpcLua {
public:
  using io_context = boost::asio::io_context;
  using stream_descriptor = boost::asio::posix::stream_descriptor;
  using tcp = boost::asio::ip::tcp;
  using udp = boost::asio::ip::udp;

  explicit RpcLua(io_context &);
  RpcLua(RpcLua &) = delete;
  RpcLua(RpcLua &&) = delete;
  ~RpcLua();

  void call(const char *func_name, JsonRpc::JsonRpcParam param1 = nullptr,
    JsonRpc::JsonRpcParam param2 = nullptr,
    JsonRpc::JsonRpcParam param3 = nullptr);

  std::string getConnectionInfo() const;

  bool alive() const;

private:
  io_context &io_ctx;

  pid_t child_pid;
  stream_descriptor child_stdin;   // 父进程写入子进程 stdin
  stream_descriptor child_stdout;  // 父进程读取子进程 stdout

  enum WaitType {
    WaitForNotification,
    WaitForResponse,
  };
  void wait(WaitType waitType, const char *method, int id);

  enum { max_length = 32768 };
  char buffer[max_length];
  std::vector<unsigned char> cborBuffer;
};
