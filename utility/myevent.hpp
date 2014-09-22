#ifndef MYEVENT_HPP
#define MYEVENT_HPP

/* quick and partial event lib. created this lib because i couldn't
 * get things to trigger using libevent (though i think it was because
 * my mistake of not connecting to the right port (forgot to htons)).
 */

#include <sys/epoll.h>
#include <map>
#include <shd-library.h>

typedef void (*mev_data_cb)(int fd, void *user_data);
typedef void (*mev_event_cb)(int fd, short what, void *user_data);


class myevent_base;

typedef enum {
    MEV_EVENT_NONE = 0,
    MEV_EVENT_CONNECTED,
    MEV_EVENT_ERROR,
    MEV_EVENT_EOF,
} mev_event_t;


class myevent_socket_t
{
public:
    /* fd should be a non-blocking socket, but not yet called
     * connect(2) on yet. use myevent_socket_t::socket_connect()
     * instead.
     *
     * if you want to get the underlying fd and then free this event
     * without affecting the fd, then call set_close_fd(false).
     *
     * with the detached fd, you can pass it to a new
     * myevent_socket_t, but you should call set_connected() on the
     * new myevent_socket_t to tell it the socket has been connected.
     */
    myevent_socket_t(myevent_base *evbase, const int fd, mev_data_cb readcb,
                     mev_data_cb writecb, mev_event_cb eventcb,
                     void *user_data);

    ~myevent_socket_t();

    /* connect the socket, and enable() the event */
    int socket_connect(struct sockaddr *address, int addrlen);
    int trigger(short what); // tell me that some event(s) occurred
    int start_monitoring(); /* similar to libevent's "event_add": need
                               to call this to monitor the event */
    int get_fd() const { return fd_; }
    void set_close_fd(const bool close_or_not) { close_fd_ = close_or_not; }

    /* use these set callbacks api to enable/disable read/write
     * monitoring, e.g., if a cb is NULL, then that part of event will
     * be disabled.
     */
    void setcb(mev_data_cb readcb, mev_data_cb writecb,
               mev_event_cb eventcb, void *user_data);
    void set_readcb(mev_data_cb readcb);
    void set_writecb(mev_data_cb writecb);
    void set_connected() { state_ = MEV_STATE_CONNECTED; }
    void set_logfn(ShadowLogFunc log) { log_ = log; }

private:

    typedef enum {
        MEV_STATE_INIT = 0,
        MEV_STATE_CONNECTING,
        MEV_STATE_CONNECTED,
        MEV_STATE_ERROR,
        MEV_STATE_CLOSED,
    } mev_state_t;

    myevent_base* evbase_;
    int fd_;
    bool close_fd_; // whether to close fd on destructor, default true
    mev_data_cb readcb_;
    mev_data_cb writecb_;
    mev_event_cb eventcb_;
    void *user_data_;
    mev_state_t state_;
    ShadowLogFunc log_;
};

class myevent_base
{
public:
    myevent_base(ShadowLogFunc log);
    ~myevent_base();

    /* the event base will not free the event. it's up to the user to
     * free the event. user should use del_event() to make the event
     * base no longer keeping track of the event.
     */
    int add_event(myevent_socket_t* mev, const uint32_t& what);


    int mod_event(const int& fd, const uint32_t& what);
    void del_event(const int& fd);

    int loop_nonblock();
    int dispatch();
    void set_logfn(ShadowLogFunc log) { log_ = log; }

private:
    int epfd_;
    std::map<int, myevent_socket_t* > fd2mev_;
    ShadowLogFunc log_;
};

#endif /* MYEVENT_HPP */
