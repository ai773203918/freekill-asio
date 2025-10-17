#pragma once

class HttpListener {
public:
  using io_context = boost::asio::io_context;
  using tcp = boost::asio::ip::tcp;

  HttpListener() = delete;
  HttpListener(HttpListener &) = delete;
  HttpListener(HttpListener &&) = delete;
  HttpListener(tcp::endpoint end);

  ~HttpListener();

  void start();

private:
  io_context io_ctx;
  tcp::endpoint endpoint;
  std::thread m_thread;

  void run();

  boost::asio::awaitable<void> listener();
};
