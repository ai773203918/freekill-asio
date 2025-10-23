// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/rpc-lua/jsonrpc.h"

static int _reqId = 1;

namespace JsonRpc {

std::map<std::string_view, JsonRpcError> errorObjects = {
    {"parse_error", {-32700, "Parse error"}},
    {"invalid_request", {-32600, "Invalid request"}},
    {"method_not_found", {-32601, "Method not found"}},
    {"invalid_params", {-32602, "Invalid params"}},
    {"internal_error", {-32603, "Internal error"}},
    {"server_error", {-32000, "Server error"}},
};

bool isStdError(const std::string_view &errorName) {
  return errorName == "parse_error" || errorName == "invalid_request" ||
         errorName == "method_not_found" || errorName == "invalid_params" ||
         errorName == "internal_error" || errorName == "server_error";
}

void JsonRpcPacket::reset() {
  id = -1;
  param_count = 0;
  error.code = 0;
  error.message = "";
  result = nullptr;
  method = "";
}

std::optional<JsonRpcError> getErrorObject(const std::string &errorName) {
  auto it = errorObjects.find(errorName);
  if (it != errorObjects.end()) {
    return it->second;
  }
  return std::nullopt;
}

JsonRpcPacket notification(const std::string_view &method,
                           const JsonRpcParam &param1,
                           const JsonRpcParam &param2,
                           const JsonRpcParam &param3)
{
  JsonRpcPacket obj;
  obj.param_count = 0;
  obj.method = method;

  if (std::holds_alternative<std::nullptr_t>(param1)) {
    return obj;
  }
  obj.param1 = param1;
  obj.param_count++;

  if (std::holds_alternative<std::nullptr_t>(param2)) {
    return obj;
  }
  obj.param2 = param2;
  obj.param_count++;

  if (std::holds_alternative<std::nullptr_t>(param3)) {
    return obj;
  }
  obj.param3 = param3;
  obj.param_count++;

  return obj;
}

JsonRpcPacket request(const std::string_view &method,
                      const JsonRpcParam &param1,
                      const JsonRpcParam &param2,
                      const JsonRpcParam &param3,
                      int id)
{
  JsonRpcPacket obj = notification(method, param1, param2, param3);
  if (id == -1) {
    id = _reqId++;
    if (_reqId > 10000000) _reqId = 1;
  }
  obj.id = id;
  return obj;
}

JsonRpcPacket response(const JsonRpcPacket &req, const JsonRpcParam &result) {
  JsonRpcPacket res;
  if (req.id >= 0) {
    res.id = req.id;
  }
  res.result = result;
  return res;
}

JsonRpcPacket responseError(const JsonRpcPacket &req, const std::string &errorName,
                          const JsonRpcParam &data) {
  auto errorOpt = getErrorObject(errorName);
  if (!errorOpt) {
    errorOpt = getErrorObject("internal_error").value();
  }

  JsonRpcError errorObj {
    .code = errorOpt->code,
    .message = errorOpt->message,
  };
  if (!std::holds_alternative<std::nullptr_t>(data)) {
    errorObj.data = data;
  }

  JsonRpcPacket res;
  res.error = errorObj;

  if (errorOpt->code == -32700 || errorOpt->code == -32600) {
    // No ID
  } else if (req.id >= 0) {
    res.id = req.id;
  }

  return res;
}

std::optional<JsonRpcPacket>
handleRequest(const RpcMethodMap &methods, const JsonRpcPacket &req) {
  if (req.method == "") {
    return responseError(req, "invalid_request");
  }

  auto it = methods.find(req.method);
  if (it == methods.end()) {
    return responseError(req, "method_not_found");
  }

  try {
    auto [success, result] = it->second(req);
    if (!success) {
      // Assume error info is in result
      return responseError(req, "invalid_params", result);
    }
    if (req.id < 0) {
      return std::nullopt; // notification
    }
    return response(req, result);
  } catch (const std::exception &e) {
    return responseError(req, "internal_error", std::string_view { e.what() });
  }
}

int getNextFreeId() { return _reqId; }

} // namespace JsonRpc
