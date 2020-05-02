/**
 * This file contains the primary logic for your server. It is responsible for
 * handling socket communication - parsing HTTP requests and sending HTTP
 * responses to the client.
 */

#include "server.hh"

#include <arpa/inet.h>
#include <dirent.h>
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <link.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
typedef void (*httprunfunc)(int ssock, const char *querystring);

#include <chrono>
#include <fstream>
#include <functional>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <tuple>
#include <vector>

#include "errors.hh"
#include "http_messages.hh"
#include "misc.hh"
#include "routes.hh"

using namespace std;

HttpResponse error404(const Socket_t &sock, HttpResponse resp);
HttpResponse contentType(std::string name, HttpResponse resp);
std::string iconType(std::string name);
HttpResponse executeCGI(const Socket_t &sock, std::string path,
                        std::string query, std::string http, HttpResponse resp,
                        int port);
HttpResponse sendFile(const Socket_t &sock, std::string file_path,
                      HttpResponse resp);
HttpResponse sendDir(const Socket_t &sock, DIR *directory, std::string path,
                     HttpResponse resp);
struct ThreadParams {
  const Server *server;
  Socket_t sock;
};

typedef struct FileEntry {
  char name[128];
  char modDate[32];
  char listing[512];
  int size;
} FileEntry;

pthread_mutex_t mutex;

void dispatchThread(ThreadParams *params, int port) {
  printf("Dispatching thread...\n");
  params->server->handle(params->sock, port);
  delete params;
}

Server::Server(SocketAcceptor const &acceptor) : _acceptor(acceptor) {}

void Server::run_linear(int port) const {
  // FILE *logFile = fopen("access.log", "w+");
  // std::time_t now = std::chrono::system_clock::to_time_t(
  //     std::chrono::system_clock::now());
  // std::string s(30, '\0');
  // std::strftime(&s[0], s.size(), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
  // flock(fileno(logFile), LOCK_EX);
  // fprintf(logFile, "%s\nServer start time: %s\n", "Server Name: Maple Tree",
  //         s.c_str());
  // flock(fileno(logFile), LOCK_UN);
  // fclose(logFile);

  while (1) {
    auto start = std::chrono::high_resolution_clock::now();
    Socket_t sock = _acceptor.accept_connection();
    handle(sock, port);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        end - start);
    std::string time_spent = std::to_string(duration.count());

    FILE *logFile = fopen("access.log", "a+");
    flock(fileno(logFile), LOCK_EX);
    fprintf(logFile, "%s ms\n\n", time_spent.c_str());

    flock(fileno(logFile), LOCK_UN);
    fclose(logFile);
  }
}

void Server::run_fork(int port) const {
  while (1) {
    Socket_t sock = _acceptor.accept_connection();
    // TODO: Task 1.4
    int pid = fork();
    if (pid == 0) {
      handle(sock, port);
    } else {
      perror("fork");
      exit(0);
    }
  }
}

void Server::run_thread(int port) const {
  // TODO: Task 1.4
  while (1) {
    Socket_t sock = _acceptor.accept_connection();
    ThreadParams *thread = new ThreadParams;
    thread->server = this;
    thread->sock = std::move(sock);

    pthread_t t1;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    pthread_create(&t1, &attr, (void *(*) (void *) ) dispatchThread,
                   (void *) thread);
  }
}

void *Server::loopthread() {
  while (1) {
    Socket_t sock = _acceptor.accept_connection();
    handle(sock, port);
  }
}

void Server::run_thread_pool(const int num_threads, int port) const {
  // pthread_t threads[num_threads];
  // pthread_attr_t attr;
  // pthread_attr_init(&attr);
  // pthread_attr_setscope(&attr, PTHREAD_SCOPE_SYSTEM);
  // pthread_mutex_init(&mutex, NULL);
  // for (int i = 0; i < num_threads - 1; i++)
  // {
  //   /* code */
  //   pthread_create(&threads[i], &attr, (void *(*)(void *))run_linear, (void
  //   *)this);
  // }

  // TODO: Task 1.4
}

// example route map. you could loop through these routes and find the first
// route which matches the prefix and call the corresponding handler. You are
// free to implement the different routes however you please
/*
std::vector<Route_t> route_map = {
  std::make_pair("/cgi-bin", handle_cgi_bin),
  std::make_pair("/", handle_htdocs),
  std::make_pair("", handle_default)
};
*/

void tokenize(std::string const &str, const char delim,
              std::vector<std::string> &out) {
  // construct a stream from the string
  std::stringstream ss(str);

  std::string s;
  while (std::getline(ss, s, delim)) {
    out.push_back(s);
  }
}

void Server::handle(const Socket_t &sock, int port) const {
  // String printed on server
  HttpRequest request;
  // TODO: implement parsing HTTP requests
  // recommendation:
  // parse_request(sock, &request);
  std::string input = sock->readline();
  std::string original = input;
  input = sock->readline();
  int count = 0;
  cout << input + "asdfasdf";
  while (input != "\r\n") {
    const char delim = ':';

    std::vector<std::string> out;
    tokenize(input, delim, out);
    if (out.size() >= 3) {
      std::string res;
      for (int i = 1; i < out.size(); i++) {
        /* code */
        res = res + ':' + out[i];
      }
      res.erase(0, 2);
      request.headers[out[0]] = res;
    } else {
      request.headers[out[0]] = out[1];
    }
    input = sock->readline();

    cout << input;
    // cout << input << endl;
  }
  HttpResponse resp;

  // input = sock->readline();
  // Store request (GET) into array
  cout << "mmmmm" << endl;
  std::vector<std::string> res;
  std::istringstream iss(original);
  for (std::string input; iss >> input;) {
    /* code */
    res.push_back(input);
  }
  request.method = res[0];
  request.request_uri = res[1];
  request.http_version = res[2];
  std::string query = "";
  if (res[1].find("?") != std::string::npos) {
    query = res[1].substr(res[1].find("?") + 1);
  }

  if (request.headers["Authorization"] == "") {
    resp.http_version = res[2];
    resp.status_code = 401;
    resp.headers["WWW-Authenticate"] = "Basic realm=\"MapleTree\"";
    // Send authentication request to browser.
    sock->write(resp.to_string());
    return;
  }
  if ((!request.request_uri.compare("/hello"))) {
    // Default Hello CS252 response
    resp.http_version = res[2];
    resp.status_code = 200;
    resp.message_body = "Hello CS252!";
    resp.headers["Connection"] = "close";
    resp.headers["Content-Type"] = "text/plain";
    resp.headers["Content-Length"] = std::to_string(resp.message_body.size());
    sock->write(resp.to_string());
    return;
  }
  if (request.headers["Authorization"].find("V29vbHk6TWFwbGU=") !=
      std::string::npos) {
    if ((!request.request_uri.compare("/hello"))) {
      // Default Hello CS252 response
      resp.http_version = res[2];
      resp.status_code = 200;
      resp.message_body = "Hello CS252!";
      resp.headers["Connection"] = "close";
      resp.headers["Content-Type"] = "text/plain";
      resp.headers["Content-Length"] = std::to_string(resp.message_body.size());
      sock->write(resp.to_string());
    } else {
      if (request.request_uri.c_str()[0] == '/') {
        int check = 0;
        std::string newDocPath;
        if (res[1] == "/") {
          newDocPath = "http-root-dir/htdocs/index.html";
        } else if (res[1].find("/icons") != std::string::npos) {
          newDocPath = "http-root-dir" + res[1];
        } else if ((res[1].find("/cgi-bin") != std::string::npos) ||
                   (res[1].find("/cgi-src") != std::string::npos)) {
          newDocPath = "http-root-dir" + res[1];
          check = 1;
        } else if (res[1].find("access.log") != std::string::npos) {
          newDocPath = "access.log";
        } else if (res[1].find("http-root-dir/htdocs/") != std::string::npos) {
          newDocPath = res[1];
        } else {
          // Should include a / inside directory.
          newDocPath = "http-root-dir/htdocs" + res[1];
        }
        // Get content type
        resp = contentType(res[1], resp);
        resp.status_code = 200;
        resp.http_version = res[2];
        if (newDocPath.front() == '/') {
          newDocPath.erase(0, 1);
        }
        if (check == 1) {
          resp = executeCGI(sock, newDocPath, query, res[2], resp, port);
          sock->write(resp.to_string());
          return;
        }

        DIR *open_dir = opendir(newDocPath.c_str());
        if (open_dir != NULL) {
          if (newDocPath.back() != '/') {
            resp.status_code = 301;
            std::string redirect = request.headers["Host"];
            cout << (int) redirect.back() << endl;
            redirect.erase(redirect.back() + 4, 2);
            std::cout << "redirecting" + redirect;
            if (request.headers["Referer"].find("https") != std::string::npos) {
              resp.headers["Location"] = "https://" + redirect + "/" +
                                         newDocPath + "/";
            } else {
              resp.headers["Location"] = "http:///" + redirect + "/" +
                                         newDocPath + "/";
            }

          } else {
            // Serve up a generated directory listing.
            std::string path = newDocPath;
            std::cout << path << std::endl;
            resp = sendDir(sock, open_dir, path, resp);
          }

          if (closedir(open_dir) < 0) {
            perror("closedir");
            exit(13);
          }
        } else {
          resp = sendFile(sock, newDocPath, resp);
        }
        sock->write(resp.to_string());
      } else {
        resp.status_code = 404;
        sock->write(resp.to_string());
      }
      // std::cout << resp.to_string() << std::endl;
    }
  } else {
    resp.status_code = 403;
    resp.http_version = res[2];
    sock->write(resp.to_string());
  }
  request.print();
  FILE *logFile = fopen("access.log", "a+");
  flock(fileno(logFile), LOCK_EX);
  fprintf(logFile, "%s %s %d ", res[0].c_str(), res[1].c_str(),
          resp.status_code);

  flock(fileno(logFile), LOCK_UN);
  fclose(logFile);
}

HttpResponse error404(const Socket_t &sock, HttpResponse resp) {
  std::cout << "Unable to open file\n";
  resp.status_code = 404;
  return resp;
}

HttpResponse contentType(std::string name, HttpResponse resp) {
  if (name.find("gif") != std::string::npos) {
    resp.headers["Content-Type"] = "image/gif";
  } else if (name.find("html") != std::string::npos || name == "/") {
    resp.headers["Content-Type"] = "text/html";
  } else if (name.find("png") != std::string::npos) {
    resp.headers["Content-Type"] = "image/png";
  } else if (name.find("svg") != std::string::npos) {
    resp.headers["Content-Type"] = "image/svg+xml";
  } else {
    resp.headers["Content-Type"] = "text/plain";
  }
  return resp;
}

HttpResponse sendFile(const Socket_t &sock, std::string newDocPath,
                      HttpResponse resp) {
  if (resp.headers["Content-Type"].find("image") == std::string::npos) {
    std::string file_path(newDocPath);
    std::ifstream ifs;
    std::stringstream ss;
    std::string input;
    ifs.open(file_path);
    if (!ifs) {
      resp = error404(sock, resp);
      return resp;
    }

    while (std::getline(ifs, input)) {
      ss << input << "\n";
    }

    ifs.close();
    resp.message_body = ss.str();
    resp.headers["Content-Length"] = std::to_string(ss.str().length());
  } else {
    std::ifstream fin(newDocPath, std::ios::binary);
    if (!fin) {
      resp = error404(sock, resp);
      return resp;
    }
    std::string data;
    data.reserve(1000000);
    std::copy(std::istreambuf_iterator<char>(fin),
              std::istreambuf_iterator<char>(), std::back_inserter(data));
    resp.message_body = data;
    resp.headers["Content-Length"] = std::to_string(resp.message_body.size());
  }
  return resp;
}

HttpResponse executeCGI(const Socket_t &sock, std::string path,
                        std::string query, std::string http, HttpResponse resp,
                        int port) {
  int fp = open(path.c_str(), O_RDONLY);

  if (fp > 0) {
    int pid = fork();
    if (pid == 0) {
      setenv("REQUEST_METHOD", "GET", 1);
      if (query.c_str() != NULL && query.c_str()[0] != '\0') {
        setenv("QUERY_STRING", query.c_str(), 1);
      }
      char protoCode[128];
      resp.http_version = http;
      resp.status_code = 200;
      if (path.find(".so") != std::string::npos) {
        dup2(port, 1);
        dup2(port, 2);
        char *const *args = NULL;
        execvp(path.c_str(), args);
        perror("execvp");
      } else {
        path.append("/");
        void *lib = dlopen(path.c_str(), RTLD_LAZY);
        cout << path << endl;
        if (lib == NULL) {
          perror("dlopen");
          exit(1);
        }

        httprunfunc libHttpRun = (httprunfunc) dlsym(lib, "httprun");
        if (libHttpRun == NULL) {
          perror("dlsym");
          exit(2);
        }

        libHttpRun(port, query.c_str());

        if (dlclose(lib) != 0) {
          perror("dlclose");
          exit(3);
        }
      }
    }
    int stat = 0;
    waitpid(pid, &stat, 0);
    return resp;
  } else {
    resp.http_version = http;
    resp.status_code = 404;
    return resp;
  }
}

HttpResponse sendDir(const Socket_t &sock, DIR *directory, std::string path,
                     HttpResponse resp) {
  resp.headers["Content-Type"] = "text/html";
  resp.status_code = 200;
  char last_char = path.back();

  char *page = (char *) malloc(sizeof(char) * 1024 * 64);
  char addressStr[128];
  char sortOrder = 'D';

  snprintf(addressStr, 127, "%s Server", "Maple Tree");

  snprintf(page, sizeof(char) * 1024 * 64,
           "<html><head> \
          <title>Index of %s</title> \
          </head><body> \
          <h1>Index of %s</h1> \
          <table> \
          <tr><th valign = \"top\"><img src=\"/icons/red_ball.gif\"></th><th> \
          <a href = \"?C=N;O=%c\">Name</a></th><th><a href = \"?C=M;O=%c\"> \
          Last modified</a></th><th><a href = \"?C=S;O=%c\">Size</a> \
          </th><th><a href = \"?C=D;O=A\">Description</a></th></tr> \
          <tr><th colspan = \"5\"><hr></th></tr> \
          <tr><td valign = \"top\"><img src = \"/icons/blue_ball.gif\"></td><td><a href = \"../\"> \
          Parent Directory</a></td><td>&nbsp;</td><td align = \"right\"> \
          - </td><td>&nbsp;</td></tr>",
           path.c_str(), path.c_str(), sortOrder, sortOrder, sortOrder);

  struct dirent **fileList;
  int fileCount = scandir(path.c_str(), &fileList, NULL, alphasort);
  FileEntry files[128];
  int entryIndex = 0;

  // Store all of the data into structures and sort it later.
  for (int i = 0; i < fileCount; i++) {
    char *filename = fileList[i]->d_name;
    if (filename[0] != '.') {  // Ignore hidden files.
      char listing[512];
      std::string tmp = filename;
      std::string iconDir = iconType(tmp);
      char fileSize[32];
      char timeStamp[64];
      std::string slash;
      struct stat attrib;

      char filePath[256];
      snprintf(filePath, 255, "%s%s", path.c_str(), filename);

      // Get file length.
      FILE *file = fopen(filePath, "r");
      if (file <= 0) {
        perror("fopen");
      }
      fseek(file, 0L, SEEK_END);
      long size = ftell(file);
      if (size == -1L) {
        perror("ftell");
      }
      fclose(file);

      // Override based on directory / file.
      if (fileList[i]->d_type == DT_DIR) {
        iconDir = "/icons/menu.gif";
        strcpy(fileSize, "-");
        slash = "/";

        size = -1;
      } else {
        snprintf(fileSize, 31, "%d", (int) size);
        slash = "";
      }
      std::string s;
      s = path + filename;
      cout << s << endl;
      stat(s.c_str(), &attrib);
      auto time = std::to_string(attrib.st_atime);

      snprintf(listing, 512,
               "<tr><td valign = \"top\"><img src=\"/%s\"></td><td><a "
               "href = \"%s\">%s</a></td><td "
               "align = \"right\">%s</td><td "
               "align = \"right\">%s</td><td>&nbsp;</td></tr>",
               iconDir.erase(0, 1).c_str(), filename, filename, time.c_str(),
               fileSize);
      strcpy(files[entryIndex].name, filename);
      strcpy(files[entryIndex].listing, listing);
      strcpy(files[entryIndex].modDate, timeStamp);
      files[entryIndex].size = (int) size;
      entryIndex++;
    }

    free(fileList[i]);
    fileList[i] = NULL;
  }

  free(fileList);
  fileList = NULL;

  for (int i = 0; i < entryIndex; i++) {
    strcat(page, files[i].listing);
  }

  char ending[512];
  snprintf(ending, 511,
           "<tr><th colspan = \"5\"><hr></th></tr> \
          </table> \
          <address>%s</address> \
          </body></html>",
           addressStr);
  strcat(page, ending);
  resp.message_body = page;
  std::cout << resp.message_body << std::endl;
  return resp;
}

std::string iconType(std::string name) {
  std::string type;
  if (name.find(".") == std::string::npos) {
    return "/icons/unknown.gif";
  }

  if (name.find("html") == std::string::npos) {
    type = "/icons/text.gif";
  } else if (name.find("gif") == std::string::npos) {
    type = "/icons/image.gif";
  } else if (name.find("png") == std::string::npos) {
    type = "/icons/image.gif";
  } else if (name.find("svg") == std::string::npos) {
    type = "/icons/image.gif";
  } else {
    type = "/icons/unknown.gif";
  }

  return type;
}
