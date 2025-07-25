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

场景2:thread想要向socket写数据，必须交给主线程完成

- 另一个thread发出send信号。
- 另一个thread发出信号后进入阻塞状态。
- 主线程回到poll后发现有信号，于是进行写入。
- 主线程告诉thread写入完毕，thread退出阻塞。

由于主线程是不能死等的，实际实现会通过asio设施，但是思想如前。

### Qt基础设施替换方案

没什么好说的，大部分可以用STL平替，但是freekill中Qt设施用的实在太多，换起来会非常痛苦。
