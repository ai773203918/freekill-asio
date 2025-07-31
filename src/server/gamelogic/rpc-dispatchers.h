// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "server/rpc-lua/jsonrpc.h"

class Player;

namespace RpcDispatchers {

extern std::string getPlayerObject(Player &p);

extern const JsonRpc::RpcMethodMap ServerRpcMethods;

}
