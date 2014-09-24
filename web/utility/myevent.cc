#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <strings.h>
#include "myassert.h"
#include <unistd.h>

#include "common.hpp"
#include "myevent.hpp"

#ifdef ENABLE_MY_LOG_MACROS
#define mylogDEBUG(fmt, ...)                                            \
    do {                                                                \
        if (log_) {                                                     \
            log_(SHADOW_LOG_LEVEL_DEBUG, __FUNCTION__, "line %u: " fmt, \
                 __LINE__, ##__VA_ARGS__);                              \
        }                                                               \
    } while (0)
#else
/* no op */
#define mylogDEBUG(fmt, ...)

#endif


myevent_base::myevent_base(ShadowLogFunc log)
    : epfd_(-1), log_(log)
{
    epfd_ = epoll_create(1);
    myassert(epfd_ != -1);
    mylogDEBUG("epfd %d", epfd_);
}

myevent_base::~myevent_base()
{
    close(epfd_);
    mylogDEBUG("closing epfd %d", epfd_);
    fd2mev_.clear();
}

int
myevent_base::loop_nonblock()
{
    /* collect the events that are ready */
    struct epoll_event epevs[32];
    const int nfds = epoll_wait(epfd_, epevs, 32, 0);
    myassert(nfds != -1);

    /* activate correct component for every socket thats ready */
    for(int i = 0; i < nfds; i++) {
        const int d = epevs[i].data.fd;
        //if (inMap(fd2mev_, d)) {
        /* XXX/we expect that the common case is that d is in the map,
         * so to avoid looking up twice, let's look up once, and check
         * for NULL result. how much will this help? dont know.
         */
        myevent_socket_t* mev = fd2mev_[d];
        if (mev) {
            mev->trigger(epevs[i].events);
        }
    }
    return 0;
}

int
myevent_base::dispatch()
{
    /* collect the events that are ready */
    struct epoll_event epevs[32];
    const int nfds = epoll_wait(epfd_, epevs, 32, -1);
    myassert(nfds != -1);

    /* activate correct component for every socket thats ready */
    for(int i = 0; i < nfds; i++) {
        const int d = epevs[i].data.fd;
        //if (inMap(fd2mev_, d)) {
        /* XXX/we expect that the common case is that d is in the map,
         * so to avoid looking up twice, let's look up once, and check
         * for NULL result. how much will this help? dont know.
         */
        myevent_socket_t* mev = fd2mev_[d];
        if (mev) {
            mev->trigger(epevs[i].events);
        }
    }
    return 0;
}

int
myevent_base::mod_event(const int& fd, const uint32_t& what)
{
    //myassert( inMap(fd2mev_, fd));

    if (! (what & (EPOLLIN | EPOLLOUT))) {
        if (log_) {
            log_(SHADOW_LOG_LEVEL_WARNING, __func__,
                 "neither IN or OUT is registered for fd= %d", fd);
        }
    }

    struct epoll_event ev;
    bzero(&ev, sizeof(ev));
    ev.events = what;
    ev.data.fd = fd;

    const int rv = epoll_ctl(epfd_, EPOLL_CTL_MOD, fd, &ev);
    if (rv) {
        if (log_) {
            log_(SHADOW_LOG_LEVEL_ERROR, __func__, "epoll_ctl error: [%s]",
                 strerror(errno));
        }
        myassert(0);
    }
    mylogDEBUG("modify event. fd = %d, what = %X, result = %d",
               ev.data.fd, ev.events, rv);
    return rv;
}

void
myevent_base::del_event(const int& fd)
{
    /* we trip this assert at the end of simulations, so just hack for
     * now and be lenient */
    // myassert(inMap(fd2mev_, fd));
    const int rv = epoll_ctl(epfd_, EPOLL_CTL_DEL, fd, NULL);
    //if (inMap(fd2mev_, fd)) {

    /* std::map allows erase of non-existing key */
    fd2mev_.erase(fd);
    //}
}

int
myevent_base::add_event(myevent_socket_t* mev, const uint32_t& what)
{
    const int fd = mev->get_fd();
    myassert(! inMap(fd2mev_, fd));
    fd2mev_[fd] = mev;

    myassert(what & (EPOLLIN | EPOLLOUT));
    
    struct epoll_event ev = {0, 0};
    ev.events = what;
    ev.data.fd = fd;

    const int rv = epoll_ctl(epfd_, EPOLL_CTL_ADD, fd, &ev);
    mylogDEBUG("add event. fd = %d, what = %X, result = %d",
               ev.data.fd, ev.events, rv);
    return rv;
}

myevent_socket_t::myevent_socket_t(
    myevent_base *evbase, const int fd, mev_data_cb readcb,
    mev_data_cb writecb, mev_event_cb eventcb,
    void *user_data)
    : evbase_(evbase), fd_(fd), close_fd_(true)
    , readcb_(readcb), writecb_(writecb), eventcb_(eventcb)
    , user_data_(user_data), state_(MEV_STATE_INIT), log_(NULL)
{
    myassert(fd_ >= 0);
    mylogDEBUG("new socket event: fd is %d", fd);
}

int
myevent_socket_t::start_monitoring()
{
    myevent_socket_t *mev = this;
    uint32_t what = 0;

    what |= mev->readcb_ ? EPOLLIN : 0;
    // epoll notifies of "connected" as a pollout, so if connecting,
    // need pollout even if user dont want writecb
    what |= (mev->state_ == MEV_STATE_CONNECTING || mev->writecb_) ? EPOLLOUT : 0;

    mylogDEBUG("add event. what = %u", what);
    int rv = mev->evbase_->add_event(mev, what);
    if(rv == -1) {
        mev->state_ = MEV_STATE_ERROR;
    }
    return rv;
}

int
myevent_socket_t::socket_connect(struct sockaddr *address, int addrlen)
{
    mylogDEBUG("begin");

    myevent_socket_t *mev = this;
    myassert (mev->state_ == MEV_STATE_INIT);

    uint32_t what = 0;

    mylogDEBUG("readcb: %X, writecb: %X, eventcb: %X",
               mev->readcb_, mev->writecb_, mev->eventcb_);
    
    mylogDEBUG("calling connect()... ");
    int rv = connect(fd_, address, addrlen);
    if (rv == 0) {
        mev->state_ = MEV_STATE_CONNECTED;
        /* it's immediately connected -> just watch for what the user
         * wants */
        what |= mev->readcb_ ? EPOLLIN : 0;
        what |= mev->writecb_ ? EPOLLOUT : 0;
        mylogDEBUG("it immediately connected");
    } else if (rv == -1) {
        if (errno == EINPROGRESS) {
            mev->state_ = MEV_STATE_CONNECTING;
            what |= mev->readcb_ ? EPOLLIN : 0;
            what |= EPOLLOUT; /* needs to know epollout as way of
                               * knowing it's connected */
            mylogDEBUG("in progress. what = 0x%X", what);
            rv = 0;
        } else {
            mev->state_ = MEV_STATE_ERROR;
            mylogDEBUG("connect error");
            goto done;
        }
    }

    rv = start_monitoring();
    if(rv == -1) {
        mev->state_ = MEV_STATE_ERROR;
    }

done:
    mylogDEBUG("done, returning rv %d", rv);
    return rv;
}

myevent_socket_t::~myevent_socket_t()
{
    mylogDEBUG("begin destructor");
    mylogDEBUG("tell evbase_ to forget about me, fd %d", fd_);
    evbase_->del_event(fd_);
    if (close_fd_) {
        mylogDEBUG("closing fd %d", fd_);
        if (fd_ != -1) {
            close(fd_);
        }
    } else {
        mylogDEBUG("NOT closing fd %d", fd_);
    }
    fd_ = -1;
}

int
myevent_socket_t::trigger(const short what)
{
    myassert((state_ == MEV_STATE_CONNECTING) || (state_ == MEV_STATE_CONNECTED));

    myevent_socket_t* mev = this;

    mylogDEBUG("trigger event fd %d, what %X", mev->fd_, what);

    if (what & EPOLLERR) {
        mylogDEBUG("POLLERR event");
        if (mev->eventcb_) {
            mev->eventcb_(mev->fd_, MEV_EVENT_ERROR, mev->user_data_);
        }
        mev->state_ = MEV_STATE_ERROR;
    }

    if (what & EPOLLRDHUP) {
        mylogDEBUG("POLLRDHUP event");
        if (mev->eventcb_) {
            mev->eventcb_(mev->fd_, MEV_EVENT_EOF, mev->user_data_);
        }
        mev->state_ = MEV_STATE_CLOSED;
    }

    if (what & EPOLLOUT) {
        mylogDEBUG("POLLOUT event");
        if (mev->state_ == MEV_STATE_CONNECTING) {
            /* can write while in state connecting -> call the
             * "eventcb"
             */
            mev->state_ = MEV_STATE_CONNECTED;
            if (mev->eventcb_) {
                mev->eventcb_(mev->fd_, MEV_EVENT_CONNECTED, mev->user_data_);
            }
        } else {
            if (mev->writecb_) {
                mev->writecb_(mev->fd_, mev->user_data_);
            }
        }
    }

    if (what & EPOLLIN) {
        mylogDEBUG("POLLIN event");
        if (mev->readcb_) {
            mev->readcb_(mev->fd_, mev->user_data_);
        }
    }

    mylogDEBUG("done");

    return 0;
}

void
myevent_socket_t::set_readcb(mev_data_cb readcb)
{
    // set the new read cb, but the other things stay the same
    setcb(readcb, writecb_, eventcb_, user_data_);
}

void
myevent_socket_t::set_writecb(mev_data_cb writecb)
{
    // set the new write cb, but the other things stay the same
    setcb(readcb_, writecb, eventcb_, user_data_);
}

void
myevent_socket_t::setcb(mev_data_cb readcb, mev_data_cb writecb,
                        mev_event_cb eventcb, void *user_data)
{
    readcb_ = readcb;
    writecb_ = writecb;
    eventcb_ = eventcb;
    user_data_ = user_data;
    // update epoll
    uint32_t what = 0;
    what |= readcb_ ? EPOLLIN : 0;
    what |= writecb_ ? EPOLLOUT : 0;
    if (readcb_) {
        myassert((what & EPOLLIN));
    }
    if (writecb_) {
        myassert((what & EPOLLOUT));
    }
    myassert(0 == evbase_->mod_event(fd_, what));
}
