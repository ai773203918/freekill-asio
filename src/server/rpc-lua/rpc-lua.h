// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "server/rpc-lua/jsonrpc.h"

using namespace JsonRpc;

class RpcLua {
public:
  explicit RpcLua(asio::io_context &);
  RpcLua(RpcLua &) = delete;
  RpcLua(RpcLua &&) = delete;
  ~RpcLua();

  void call(const char *func_name, JsonRpcParam param1 = nullptr, JsonRpcParam param2 = nullptr, JsonRpcParam param3 = nullptr);

  std::string getConnectionInfo() const;

private:
  asio::io_context &io_ctx;

  pid_t child_pid;
  asio::posix::stream_descriptor child_stdin;   // 父进程写入子进程 stdin
  asio::posix::stream_descriptor child_stdout;  // 父进程读取子进程 stdout

  enum { max_length = 131072 };
  char buffer[max_length];
};
