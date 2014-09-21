#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include <unistd.h> /* close */
#include <string.h> /* memset */
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <netdb.h>
#include <time.h>
#include <shd-library.h>

#include "myevent.hpp"

#include <map>
#include <string>
#include <queue>
#include <set>

class webserver_t
{
public:
    webserver_t();
    ~webserver_t();

    void start(int argc, char *argv[]);
    void activate(const bool blocking);
    void on_readable();

    const uint32_t instNum_; // monotonic id of this webserver obj
private:

    static uint32_t nextInstNum;

    myevent_base* evbase_;
    myevent_socket_t* listenev_;
    int listenfd_;
    std::string docroot_;
};

void webserver_start(webserver_t* b, int argc, char** argv);
void webserver_free(webserver_t* b);
void webserver_activate(webserver_t* b);

#endif /* WEBSERVER_HPP */
