// SPDX-License-Identifier: GPL-3.0-or-later

// jsonrpc.h
// 让AI就着jsonrpc.lua改的 懒得优化写法了
// 由于性能出现重大瓶颈，改为CBOR格式，反正这个也是Qt自带支持
#pragma once

namespace JsonRpc {

enum JsonKeys {
  // jsonrpc
  JsonRpc = 100,
  Method,
  Params,
  Error,
  Id,
  Result,

  // jsonrpc.error
  ErrorCode = 200,
  ErrorMessage,
  ErrorData,
};

typedef std::variant<int, int64_t, std::string_view, bool, std::nullptr_t> JsonRpcParam;

struct JsonRpcError {
  int code = 0;
  const char *message;
  JsonRpcParam data = nullptr;
};

struct JsonRpcPacket {
  // const char *jsonrpc; // 必定是2.0 不加
  int id = -1; // 负数表示没有id 是notification
  int param_count = 0;

  std::string_view method;
  JsonRpcParam param1 = nullptr;
  JsonRpcParam param2 = nullptr;
  JsonRpcParam param3 = nullptr;
  JsonRpcParam param4 = nullptr;
  JsonRpcParam param5 = nullptr;

  JsonRpcError error;
  JsonRpcParam result = nullptr;
};

using RpcMethod =
    std::function<std::pair<bool, JsonRpcParam>(const JsonRpcPacket &)>;
using RpcMethodMap = std::map<std::string_view, RpcMethod>;

extern std::map<std::string_view, JsonRpcError> errorObjects;

// 判断是否是标准错误
bool isStdError(const std::string_view &errorName);

// 获取错误对象
std::optional<JsonRpcError> getErrorObject(const std::string &errorName);

JsonRpcPacket notification(const std::string_view &method,
                           const JsonRpcParam &param1 = nullptr,
                           const JsonRpcParam &param2 = nullptr,
                           const JsonRpcParam &param3 = nullptr);
JsonRpcPacket request(const std::string_view &method,
                      const JsonRpcParam &param1 = nullptr,
                      const JsonRpcParam &param2 = nullptr,
                      const JsonRpcParam &param3 = nullptr,
                      int id = -1);
JsonRpcPacket response(const JsonRpcPacket &req, const JsonRpcParam &result);
JsonRpcPacket responseError(const JsonRpcPacket &req, const std::string &errorName,
                          const JsonRpcParam &data = nullptr);

std::optional<JsonRpcPacket>
handleRequest(const RpcMethodMap &methods, const JsonRpcPacket &req);

// 获取下一个可用的请求ID
int getNextFreeId();

} // namespace JsonRpc
