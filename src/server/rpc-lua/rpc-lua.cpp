// SPDX-License-Identifier: GPL-3.0-or-later

#include "server/rpc-lua/rpc-lua.h"
#include "server/rpc-lua/jsonrpc.h"

#include "core/util.h"

#include <unistd.h>

RpcLua::RpcLua(asio::io_context &ctx) : io_ctx { ctx },
  child_stdin { ctx }, child_stdout { ctx }
{
  // env.insert("FK_RPC_MODE", "cbor");

  int stdin_pipe[2];  // [0]=read, [1]=write
  int stdout_pipe[2]; // [0]=read, [1]=write
  if (::pipe(stdin_pipe) == -1 || pipe(stdout_pipe) == -1) {
    throw std::runtime_error("Failed to create pipes");
  }

  pid_t pid = fork();
  if (pid == 0) { // child
    // 关闭父进程用的 pipe 端
    ::close(stdin_pipe[1]);  // 关闭父进程的写入端（子进程只读 stdin）
    ::close(stdout_pipe[0]); // 关闭父进程的读取端（子进程只写 stdout）

    // 重定向 stdin/stdout
    ::dup2(stdin_pipe[0], STDIN_FILENO);   // 子进程的 stdin ← 父进程的写入端
    ::dup2(stdout_pipe[1], STDOUT_FILENO); // 子进程的 stdout → 父进程的读取端
    ::close(stdin_pipe[0]);
    ::close(stdout_pipe[1]);

    sigset_t newmask, oldmask;
    sigemptyset(&newmask);
    sigaddset(&newmask, SIGINT); // 阻塞 SIGINT
    sigprocmask(SIG_BLOCK, &newmask, &oldmask);

    if (int err = ::chdir("packages/freekill-core"); err != 0) {
      std::exit(err);
    }

    ::setenv("FK_RPC_MODE", "cbor", 1);
    ::execlp("lua5.4", "lua5.4", "lua/server/rpc/entry.lua", nullptr);

    std::exit(EXIT_FAILURE);
  } else if (pid > 0) { // 父进程
    // 转下文
  } else {
    throw std::runtime_error("Failed to fork process");
  }

  // 关闭子进程用的 pipe 端
  close(stdin_pipe[0]);   // 关闭子进程的读取端（父进程只写 stdin）
  close(stdout_pipe[1]);  // 关闭子进程的写入端（父进程只读 stdout）

  // 使用 asio 包装 pipe
  child_stdin = { io_ctx, stdin_pipe[1] };
  child_stdout = { io_ctx, stdout_pipe[0] };

  // asio::posix::stream_descriptor child_stdout;  // 父进程读取子进程 stdout
  size_t length = child_stdout.read_some(asio::buffer(buffer, max_length));
  if (true) {
#ifdef RPC_DEBUG
    spdlog::debug("Me <-- {}", toHex({ buffer, length }));
#endif
  } else {
    // TODO: throw, then retry
    spdlog::critical("Lua5.4 closed too early.");
    // spdlog::critical("  stderr: %s", qUtf8Printable(process->readAllStandardError()));
    // spdlog::critical("  stdout: %s", qUtf8Printable(process->readAllStandardOutput()));
  }
}

RpcLua::~RpcLua() {
  call("bye");
#ifdef RPC_DEBUG
  // spdlog::debug("Me --> %ls", qUtf16Printable(Color("Say goodbye", fkShell::Blue)));
#endif
  // if (socket->waitForReadyRead(15000)) {
  //   auto msg = socket->readLine();
#ifdef RPC_DEBUG
  //   spdlog::debug("Me <-- %s", qUtf8Printable(msg));
#endif

  //   auto process = dynamic_cast<QProcess *>(socket);
  //   if (process) process->waitForFinished();
  // } else {
  //   // 不管他了，杀了
  // }

  // SIGKILL!!
  // delete socket;
}

void RpcLua::dofile(const char *path) {
  call("dofile", path);
}

void RpcLua::call(const char *func_name, JsonRpcParam param1, JsonRpcParam param2) {
  // QMutexLocker locker(&io_lock);

  auto req = JsonRpc::request(func_name, param1, param2);
  auto id = req.id;
  sendPacket(req);

  // while (socket->bytesAvailable() > 0 || socket->waitForReadyRead(15000)) {
  //   if (!socket->isOpen()) break;

  //   auto bytes = socket->readAll();
  //   QCborValue doc;
  //   do {
  //     QCborStreamReader reader(bytes);
  //     doc = QCborValue::fromCbor(reader);
  //     auto err = reader.lastError();
  //     // spdlog::debug("  *DBG* Me <-- %ls {%s}", qUtf16Printable(err.toString()), qUtf8Printable(bytes.toHex()));
  //     if (err == QCborError::EndOfFile) {
  //       socket->waitForReadyRead(100);
  //       bytes += socket->readAll();
  //       reader.clear(); reader.addData(bytes);
  //       continue;
  //     } else if (err == QCborError::NoError) {
  //       break;
  //     } else {
#ifdef RPC_DEBUG
  //       spdlog::debug("Me <-- Unrecoverable reader error: %ls", qUtf16Printable(err.toString()));
#endif
  //       return QVariant();
  //     }
  //   } while (true);

  //   auto packet = doc.toMap();
  //   if (packet[JsonRpc::JsonRpc].toByteArray() == "2.0" && packet[JsonRpc::Id] == id && !packet[JsonRpc::Method].isByteArray()) {
#ifdef RPC_DEBUG
  //     spdlog::debug("Me <-- %s", qUtf8Printable(mapToJson(packet, true)));
#endif
  //     return packet[JsonRpc::Result].toVariant();
  //   } else {
#ifdef RPC_DEBUG
  //     spdlog::debug("  Me <-- %s", qUtf8Printable(mapToJson(packet, true)));
#endif
  //     auto res = JsonRpc::serverResponse(methods, packet);
  //     if (res) {
  //       socket->write(res->toCborValue().toCbor());
  //       socket->waitForBytesWritten(15000);
#ifdef RPC_DEBUG
  //       spdlog::debug("  Me --> %s", qUtf8Printable(mapToJson(*res, false)));
#endif
  //     }
  //   }
  // }

#ifdef RPC_DEBUG
  // spdlog::debug("Me <-- IO read timeout. Is Lua process died?");
  // spdlog::debug() << dynamic_cast<QProcess *>(socket)->readAllStandardError();
#endif
}

/*
QString RpcLua::getConnectionInfo() const {
  auto process = dynamic_cast<QProcess *>(socket);

  if (process) {
    auto pid = process->processId();
    auto ret = QString("PID %1").arg(pid);

#ifdef Q_OS_LINUX
    // 若为Linux，附送RSS信息
    QFile f(QString("/proc/%1/statm").arg(pid));
    if (f.open(QIODevice::ReadOnly)) {
      const QList<QByteArray> parts = f.readAll().split(' ');
      const long pageSize = sysconf(_SC_PAGESIZE);
      auto mem_mib = (parts[1].toLongLong() * pageSize) / (1024.0 * 1024.0);
      ret += QString::asprintf(" (RSS = %.2f MiB)", mem_mib);
    }
#endif

    return ret;
  } else {
    return "unknown";
  }
}
*/

// TODO 用cbor_stream_encode或者徒手发送
void RpcLua::sendPacket(const JsonRpcPacket &packet) {
  // socket->write(req.toCborValue().toCbor());
  // socket->waitForBytesWritten(15000);
#ifdef RPC_DEBUG
  // spdlog::debug("Me --> %s", qUtf8Printable(mapToJson(req, false)));
#endif

}
