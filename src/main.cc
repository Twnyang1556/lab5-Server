/**
 * This file parses the command line arguments and correctly
 * starts your server. You should not need to edit this file
 */

#include <sys/file.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <iostream>

#include "server.hh"
#include "socket.hh"
#include "tcp.hh"
#include "tls.hh"

enum concurrency_mode {
  E_NO_CONCURRENCY = 0,
  E_FORK_PER_REQUEST = 'f',
  E_THREAD_PER_REQUEST = 't',
  E_THREAD_POOL = 'p'
};

auto time_start = std::chrono::high_resolution_clock::now();

extern "C" void signal_handler(int signal) {
  // while (waitpid(-1, NULL, WNOHANG) > 0)
  //     ;
  auto time_end = std::chrono::high_resolution_clock::now();
  FILE *logFile = fopen("access.log", "a+");
  auto duration = std::chrono::duration_cast<std::chrono::seconds>(time_end -
                                                                   time_start);
  std::time_t now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());

  std::string s(30, '\0');
  std::strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  flock(fileno(logFile), LOCK_EX);
  fprintf(logFile, "Server ended at: %s\nUptime is: %s seconds\n", s.c_str(),
          std::to_string(duration.count()).c_str());
  flock(fileno(logFile), LOCK_UN);
  fclose(logFile);
  exit(0);
}

extern "C" void handle_zombie(int) {
  int status;
  pid_t pid;

  pid = wait3(&status, 0, NULL);

  while (waitpid(-1, NULL, WNOHANG) > 0)
    ;
}

int main(int argc, char **argv) {
  // Cleanup zombie processes.
  struct sigaction zombieslayer;
  zombieslayer.sa_handler = handle_zombie;
  sigemptyset(&zombieslayer.sa_mask);
  zombieslayer.sa_flags = SA_RESTART;

  if (sigaction(SIGCHLD, &zombieslayer, NULL)) {
    perror("sigaction");
    exit(11);
  }

  struct rlimit mem_limit = {.rlim_cur = 40960000, .rlim_max = 91280000};
  struct rlimit cpu_limit = {.rlim_cur = 300, .rlim_max = 600};
  struct rlimit nproc_limit = {.rlim_cur = 50, .rlim_max = 100};
  if (setrlimit(RLIMIT_AS, &mem_limit)) {
    perror("Couldn't set memory limit\n");
  }
  if (setrlimit(RLIMIT_CPU, &cpu_limit)) {
    perror("Couldn't set CPU limit\n");
  }
  if (setrlimit(RLIMIT_NPROC, &nproc_limit)) {
    perror("Couldn't set NPROC limit\n");
  }

  struct sigaction sa;
  sa.sa_handler = signal_handler;
  sigemptyset(&sa.sa_mask);
  sigaction(SIGINT, &sa, NULL);
  enum concurrency_mode mode = E_NO_CONCURRENCY;
  char use_https = 0;
  int port_no = 0;
  int num_threads = 0;  // for use when running in pool of threads mode

  char usage[] = "USAGE: myhttpd [-f|-t|-pNUM_THREADS] [-s] [-h] PORT_NO\n";

  if (argc == 1) {
    fputs(usage, stdout);
    return 0;
  }

  int c;
  while ((c = getopt(argc, argv, "hftp:s")) != -1) {
    switch (c) {
      case 'h':
        fputs(usage, stdout);
        return 0;
      case 'f':
      case 't':
      case 'p':
        if (mode != E_NO_CONCURRENCY) {
          fputs("Multiple concurrency modes specified\n", stdout);
          fputs(usage, stderr);
          return -1;
        }
        mode = (enum concurrency_mode) c;
        if (mode == E_THREAD_POOL) {
          num_threads = stoi(std::string(optarg));
        }
        break;
      case 's':
        use_https = 1;
        break;
      case '?':
        if (isprint(optopt)) {
          std::cerr << "Unknown option: -" << static_cast<char>(optopt)
                    << std::endl;
        } else {
          std::cerr << "Unknown option" << std::endl;
        }
        // Fall through
      default:
        fputs(usage, stderr);
        return 1;
    }
  }

  if (optind > argc) {
    std::cerr << "Extra arguments were specified" << std::endl;
    fputs(usage, stderr);
    return 1;
  } else if (optind == argc) {
    std::cerr << "Port number must be specified" << std::endl;
    return 1;
  }

  port_no = atoi(argv[optind]);
  printf("%d %d %d %d\n", mode, use_https, port_no, num_threads);
  time_start = std::chrono::high_resolution_clock::now();

  SocketAcceptor *acceptor;
  if (use_https) {
    acceptor = new TLSSocketAcceptor(port_no);
  } else {
    acceptor = new TCPSocketAcceptor(port_no);
  }

  FILE *logFile = fopen("access.log", "w+");
  std::time_t now = std::chrono::system_clock::to_time_t(
      std::chrono::system_clock::now());
  std::string s(30, '\0');
  std::strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  flock(fileno(logFile), LOCK_EX);
  fprintf(logFile, "%s\nServer start time: %s\n", "Server Name: Maple Tree\n",
          s.c_str());
  fprintf(logFile, "Client is at: %s\n", std::to_string(port_no).c_str());
  flock(fileno(logFile), LOCK_UN);
  fclose(logFile);

  Server server(*acceptor);
  switch (mode) {
    case E_FORK_PER_REQUEST:
      server.run_fork(port_no);
      break;
    case E_THREAD_PER_REQUEST:
      server.run_thread(port_no);
      break;
    case E_THREAD_POOL:
      server.run_thread_pool(num_threads, port_no);
      break;
    default:
      server.run_linear(port_no);
      break;
  }
  delete acceptor;
}
