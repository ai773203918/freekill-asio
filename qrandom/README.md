唉，Lua依赖着QRandomGenerator，因此这里为QRandomGenerator开了一个接口

```sh
$ make
$ sudo make install
```

如果你实在没有sudo的话，可以在启动freekill-asio的时候指定`LUA_CPATH`环境变量：

```sh
$ LUA_CPATH=';;/path/to/qrandom' ./freekill-asio
```

这会给lua的`package.cpath`变量加入qrandom编译目录，好让他正常require到so文件。
