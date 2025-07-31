freekill-asio
===============

还就是那个C++的新月杀服务端，但是：

- Qt不是面向服务端的库，直接踹了
- SWIG不需要了，本服务端必须使用rpc与Lua通信
- 跨平台不需要了，专为Linux服务端编写

没有了Qt，我们引入asio库和libcbor。选择它们的原因是因为Debian包管理器里面有。

因此本项目的所有依赖为：

- std
- libasio
- libcbor
- cJSON
- sqlite3
- libgit2
- libreadline
- libspdlog (！版本要求较高)
- libcrypto (OpenSSL)

由于大量copy了FreeKill项目的源码，本项目还是GPLv3协议开源。

构建与运行
------------

__________________

重构的规划
-------------

### 单线程内的信号槽替换方案

单一线程中emit信号就是直接调用槽函数。直接改成普通的函数调用。

具体而言：

- 将signal全部改成`<sig_name>_callback`
- 将`QObject::connect`全部改成`set_<sig_name>_callback`

### 多线程之间信号槽替换方案

原本FreeKill服务端主要是两个线程，线程之间通过Qt信号槽处理，具体而言：

场景1:主线程告诉thread有消息可读

- 主线程处理完数据后，emit某个signal（主线程不阻塞）
- 因为slot在另一个线程，那个signal及其参数置入某个Qt消息队列
- 另一个thread完成dispatcher返回poll中时，发现有信号，于是不断处理它们

```cpp
Mutex mutex = 1;
Semaphore full = 0;
Queue msgqueue;

// 主线程
void sendSignal(auto msg) {
    P(mutex);
    msgqueue.enqueue(msg);
    V(mutex);

    V(full);
}

// 子线程的任务就是不断等待
void thread_main() {
    while (true) {
        P(full);

        P(mutex);
        auto msg = msgqueue.dequeue();
        V(mutex);

        handle(msg);
    }
}
```

但是这是直接基于std的做法，而我们最好要能用到asio的设施（尤其是异步计时器，实现QTimer行为），因此需要针对asio库的情况重新设计一下新线程的实现。

```cpp
void thread_main() {
    // 这个io_context相当于Qt中创建新的EventLoop
    // 表示我们新线程是一个单独的处理循环 而非线程池的一分子
    // 实际中可能是一个对象成员
    io_context io_ctx2;

    io_ctx2.run();
}

// 主线程向子线程传递信号
// ...
asio::post(io_ctx2 /*, <lambda> */);

```

场景2:thread想要向socket写数据，必须交给主线程完成

这个一般用于thread想要通过socket发信息的问题。直接post的话会把内容塞到内部某个队列中，
若不等待的话可能导致向已关闭的socket发信息，在asio这边就不是warning而是直接抛异常了，必须阻塞。

- 另一个thread发出send信号。
- 另一个thread发出信号后进入阻塞状态。
- 主线程回到poll后发现有信号，于是进行写入。
- 主线程告诉thread写入完毕，thread退出阻塞。

可能可以基于std里面的promise-future以及asio::post实现异步等待与跨线程发信号

```cpp
// ... 假设在Router::sendMessage中
if (i_am_main_thread) {
    socket->send( { msg.data(), msg.size() } );
} else {
    std::promise<bool> p;
    auto f = p.get_future();

    asio::post(main_io_context, [&](){
        socket->send( /* ... */ );
        p.set_value(true);
    });

    f.get();
}
```

从以上两个伪代码可以看出需要再实现这两点：

- 可以判断当前线程是否为主线程
- 可以拿到当前线程对应的io_context

场景3:基于锁的临界区访问

这个一般是基于QMutex确保这个变量同时只会被一个进程读写。

直接用std::mutex平替即可

做起来好像也不是那么痛苦，为数不多比较像人的地方

### Qt基础设施替换方案

没什么好说的，大部分可以用STL平替，但是freekill中Qt设施用的实在太多，换起来会非常痛苦。

#### QTimer

见于Room中设置Timer与取消Timer，用于Lua中实现delay以及携带一个超时等待用户答复。

```cpp
// setRequestTimer
request_timer = asio::steady_timer(io_ctx2);
request_timer.expires_after(std::chrono::milliseconds(ms));
request_timer.async_wait([&](){ /* 唤起房间... */ });

// destroyRequestTimer
request_timer.cancel();
```

#### QProcess

见于RoomThread中通过stdio (piped) 与Lua子进程进行rpc通信的场景。

可惜的是std和无boost的asio并没有对子进程的支持。但我们是Linux专用，因此子进程的创建可以用系统调用来做，我们要读的stdout, stderr和要写的stdin自己手动弄个基于文件描述符的、asio包装过的。

- 父进程要维护子进程的pid，在析构时杀掉子进程
- 父进程要维护子进程的文件描述符（通过`asio::posix::stream_descriptor(io_ctx2, ::dup(fd))`包装）
- 包装后通过`read_some`和`write_some`实现阻塞式的读写

```cpp

```
