# 使用 Alpine Linux 作为基础镜像，它是一个非常轻量级的 Linux 发行版
FROM alpine:latest

# 设置工作目录
WORKDIR /app

# 安装必要的工具
RUN apk add --no-cache \
    curl \
    tar \
    && rm -rf /var/cache/apk/*

# 获取最新版本号和当前架构
RUN ARCH=$(uname -m) && \
    if [ "$ARCH" = "x86_64" ]; then \
        ARCH="amd64"; \
    elif [ "$ARCH" = "aarch64" ]; then \
        ARCH="aarch64"; \
    elif [ "$ARCH" = "armv7l" ]; then \
        ARCH="armv7"; \
    elif [ "$ARCH" = "armhf" ]; then \
        ARCH="armhf"; \
    elif [ "$ARCH" = "loongarch64" ]; then \
        ARCH="loongarch64"; \
    elif [ "$ARCH" = "riscv64" ]; then \
        ARCH="riscv64"; \
    elif [ "$ARCH" = "i386" ] || [ "$ARCH" = "i686" ]; then \
        ARCH="x86"; \
    fi && \
    \
    # 获取最新版本号
    LATEST_VERSION=$(curl -s https://api.github.com/repos/Qsgs-Fans/freekill-asio/releases/latest | grep '"tag_name":' | sed -E 's/.*"([^"]+)".*/\1/' | sed 's/v//') && \
    \
    # 下载对应架构的包
    curl -L -o freekill-asio.tar.gz "https://github.com/Qsgs-Fans/freekill-asio/releases/download/v${LATEST_VERSION}/freekill-asio-static-v${LATEST_VERSION}-${ARCH}.tar.gz" && \
    \
    # 创建临时目录并解压
    mkdir temp && \
    tar -xzf freekill-asio.tar.gz -C temp && \
    \
    # 将解压后的文件夹内容移动到当前目录
    mv temp/*/* . && \
    \
    # 清理
    rm -rf temp freekill-asio.tar.gz

# 设置启动命令
CMD ["./freekill-asio"]
