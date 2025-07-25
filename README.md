freekill-asio
===============

还就是那个C++的新月杀服务端，但是：

- Qt不是面向服务端的库，直接踹了
- SWIG不需要了，本服务端必须使用rpc与Lua通信
- 跨平台不需要了，专为Linux服务端编写

没有了Qt，我们引入asio库和libcbor。选择它们的原因是因为Debian包管理器里面有。

因此本项目的所有依赖为：

- std
- libcbor
- libasio
- sqlite3
- libgit2
- libreadline
- libspdlog
- libcrypto (OpenSSL)

由于大量copy了FreeKill项目的源码，本项目还是GPLv3协议开源。
