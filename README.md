freekill-asio
==============

![](https://img.shields.io/github/repo-size/Qsgs-Fans/freekill-asio?color=green)
![](https://img.shields.io/github/languages/top/Qsgs-Fans/freekill-asio?color=red)
![](https://img.shields.io/github/license/Qsgs-Fans/freekill-asio)
![](https://img.shields.io/github/v/tag/Qsgs-Fans/freekill-asio)
![](https://img.shields.io/github/issues/Qsgs-Fans/freekill-asio)
[![Discord](https://img.shields.io/badge/chat-discord-blue)](https://discord.gg/tp35GrQR6v)
![](https://img.shields.io/github/stars/Qsgs-Fans/freekill-asio?style=social)

freekill-asio是一个移除了Qt依赖的freekill服务端，力求不浪费服务器的性能。

项目文档详见 https://fkbook-all-in-one.readthedocs.io/zh-cn/latest/for-devs/asio/index.html

**已知问题 & TODO**

- [ ] 优化`router->notify`和`router->request`，现版本相当于将消息复制了一遍，实验一下能不能直接分几段sendMessage以规避消息字符串的拷贝
- [ ] `RoomBase::handlePacket`里面开发频率限制功能（操作太快啦~）
- [ ] 在线程安全的前提下，考虑引入（基于asio的）http库实现某些restful API

直接下载运行
--------------

去下载最新Release，选择适合自己的CPU架构（例如 `freekill-asio-static-0.0.1-amd64.tar.gz` ）

然后找一台Linux服务器，将包复制进去后，执行：

```sh
$ tar xf freekill-asio-static-0.0.1-amd64.tar.gz
$ cd freekill-asio-static
$ ./freekill-asio
```

恭喜你顺利运行起了FreeKill服务端。

手动构建运行
--------------

本服务端只支持Linux，且手动构建运行至少需要Debian11(需要至少g++ 10.2，用到了一些C++20特性)。推荐用尽可能新的版本。

### 安装依赖

**Debian 13:**

```sh
$ sudo apt install git g++ cmake pkg-config
$ sudo apt install libasio-dev libssl-dev libcbor-dev libcjson-dev libsqlite3-dev libgit2-dev libreadline-dev libspdlog-dev
```

其余版本较新的发行版（如Arch、Kali等）安装依赖方式与此大同小异。

**Debian 11, 12:**

推荐将整个系统升级到Debian 13，或者按下面所述单独手动编译安装spdlog，因为对spdlog库版本要求稍微较高：

```sh
$ sudo apt install git g++ cmake pkg-config
$ sudo apt install libasio-dev libssl-dev libcbor-dev libcjson-dev libsqlite3-dev libgit2-dev libreadline-dev

$ git clone https://github.com/gabime/spdlog.git
$ cd spdlog && mkdir build && cd build
$ cmake .. && cmake --build .
$ sudo cmake --install .
```

### 另外安装Lua的依赖

freekill-asio并不直接将Lua嵌入到自己执行，而是将Lua作为子进程执行，这需要系统安装了lua5.4。

**Debian 11, 12, 13:**

```sh
$ sudo apt install lua5.4 lua-socket lua-filesystem
```

> **Debian11 附注:**
>
> 由于无法安装Qt6Core，改为安装`qtbase5-dev`包。
>
> 由于apt提供的lua-socket没有5.4版，需要手动处理。apt提供的luarocks亦没有5.4版，因此我们全部都需要手动安装：
>
> ```sh
> $ sudo apt install build-essential libreadline-dev liblua5.4-dev unzip
> $ wget https://luarocks.github.io/luarocks/releases/luarocks-3.12.2.tar.gz
> $ tar xf luarocks-3.12.2.tar.gz && cd luarocks-3.12.2
> $ ./configure
> $ make
> $ sudo make install
> $ sudo luarocks install LuaSocket
> ```
>
> 结论是不要用Debian11安装。直接用13吧，所有全部apt搞定。

### 构建

上文安装Lua依赖时已经clone过仓库了，下面需要在仓库的根目录下执行：

```sh
$ mkdir build && cd build
$ cmake ..
$ make
```

### 运行

和Freekill一样，freekill-asio不能直接在build目录下运行，需要在repo目录下运行：

```sh
$ ln -s build/freekill-asio
$ ./freekill-asio
```

这样就启动了freekill-asio服务器，界面与FreeKill类似，但是必须安装freekill-core，否则无法游玩（实际上在原版Freekill服务器撒谎那个这个包也是必须安装的）

```
[freekill-asio v0.0.1] Welcome to CLI. Enter 'help' for usage hints.
fk-asio> install https://gitee.com/Qsgs-Fans/freekill-core
```

然后如同普通的FreeKill服务器一样安装其他需要的包即可。

平台支持
-----------

仅测试过Linux (GCC 10+)

- Linux (Debian 11+, Arch)

若为Release中的静态编译版，则测试过了：

- Linux (Debian 7)

开源许可
-----------

本项目是基于FreeKill的源码重新开发的，依照GPL协议继续使用GPLv3许可。
