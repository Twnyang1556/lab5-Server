#ifndef INCLUDE_SERVER_HH_
#define INCLUDE_SERVER_HH_

#include "socket.hh"

class Server {
 private:
  SocketAcceptor const &_acceptor;
  int port;

 public:
  explicit Server(SocketAcceptor const &acceptor);
  void run_linear(int port) const;
  void run_fork(int port) const;
  void run_thread_pool(const int num_threads, int port) const;
  void run_thread(int port) const;
  void *loopthread();
  // void parse_request(const Socket_t &sock, HttpRequest *const request);
  // void error404(const Socket_t &sock, std::string http, HttpResponse *const
  // resp);
  void handle(const Socket_t &sock, int port) const;
};

#endif  // INCLUDE_SERVER_HH_
