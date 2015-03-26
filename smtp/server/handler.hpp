#ifndef HANDLER_HPP
#define HANDLER_HPP

#include <unistd.h> /* close */
#include <string.h> /* memset */
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <netdb.h>
#include <glib.h>
#include <glib/gprintf.h>
#include "myassert.h"
#include <time.h>
#include <shd-library.h>
#include <event2/event.h>
#include <event2/buffer.h>

#include "myevent.hpp"

#include <map>
#include <list>
#include <string>
#include <queue>
#include <set>

class Handler
{
public:
    Handler(myevent_base* evbase, const std::string& docroot,
            const int client_fd);
    ~Handler();

    void recv_from_client();
    void send_to_client();
    void on_client_sock_eof();
    void on_client_sock_error();

    const uint32_t instNum_; // monotonic id of this handler

private:

    /* schedule later self-destruction */
    void deleteLater();

    myevent_base* evbase_; /* borrowed. do not free */
    static uint32_t nextInstNum;

    const std::string docroot_;
    myevent_socket_t* cliSideSock_ev_;
    int cliSideSock_;
    struct evbuffer* inbuf_;
    struct evbuffer* outbuf_;
    int active_fd_; /* of the file requested, actively being served,
                     * or -1 */

    /* use a flag to avoid unnecessarily -- though not affecting
     * correctness -- calling the event's methods()
     */
    bool write_to_client_enabled_;
    void enable_write_to_client_();
    void disable_write_to_client_();

    bool read_from_client_enabled_;
    void enable_read_from_client_();
    void disable_read_from_client_();

    /* if this function wants more data (in inbuf_) for processing, it
     * will enable_read_from_client_(), and return true. also, if
     * submitted_req_queue_ is not empty, it will
     * enable_write_to_client_().
     */
    bool process_inbuf_();

    uint16_t peer_port_;

    /* for debugging / asserting, for the current active response */
    size_t numRespBodyBytesExpectedToSend_;
    size_t numRespBytesSent_;
    size_t numBodyBytesRead_;

#ifdef TEST_BYTE_RANGE
    size_t numRespMetaBytes_;
#endif

};

#endif /* HANDLER_HPP */
