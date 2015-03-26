/* handles a connection with the client. support max pipeline size
 * MAX_PIPELINE_REQS.
 */

#include "handler.hpp"
#include "common.hpp"

#include <getopt.h>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <vector>
#include <algorithm> /* for std::find() */
#include <string>

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <boost/lexical_cast.hpp>

using std::string;
using boost::lexical_cast;

extern ShadowLogFunc logfn;
extern ShadowCreateCallbackFunc scheduleCallback;

#define MAX_PIPELINE_REQS (1)

#ifdef ENABLE_MY_LOG_MACROS
/* "inst" stands for instance, as in, instance of a class */
#define loginst(level, inst, fmt, ...)                                  \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, hndlr= %d): " fmt, \
              __LINE__, (inst)->instNum_, ##__VA_ARGS__);               \
    } while (0)

/* like loginst, but shortcut "this" as instance */
#define logself(level, fmt, ...)                                        \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, hndlr= %d): " fmt, \
              __LINE__, (this)->instNum_, ##__VA_ARGS__);               \
    } while (0)

#else
/* no-ops */

#define loginst(level, inst, fmt, ...)

#define logself(level, fmt, ...)

#endif


uint32_t Handler::nextInstNum = 0;

namespace {

void
mev_readcb(int fd, void *ptr)
{
    Handler *h = (Handler *)(ptr);
    h->recv_from_client();
}

void
mev_writecb(int fd, void *ptr)
{
    Handler *h = (Handler *)(ptr);
    h->send_to_client();
}

void
mev_eventcb(int fd, short events, void *ptr)
{
    Handler *h = (Handler*)(ptr);
    if(events & MEV_EVENT_CONNECTED) {
        myassert(0);
    } else if(events & MEV_EVENT_EOF) {
        h->on_client_sock_eof();
    } else if(events & (MEV_EVENT_ERROR)) {
        h->on_client_sock_error();
    }
}

void
delete_handler(void* ptr)
{
    delete ((Handler*)ptr);
}

} // namespace

void
Handler::enable_write_to_client_()
{
    if (!write_to_client_enabled_) {
        cliSideSock_ev_->set_writecb(mev_writecb);
        write_to_client_enabled_ = true;
    }
}

void
Handler::disable_write_to_client_()
{
    if (write_to_client_enabled_) {
        cliSideSock_ev_->set_writecb(NULL);
        write_to_client_enabled_ = false;
    }
}

void
Handler::enable_read_from_client_()
{
    if (!read_from_client_enabled_) {
        cliSideSock_ev_->set_readcb(mev_readcb);
        read_from_client_enabled_ = true;
    }
}

void
Handler::disable_read_from_client_()
{
    if (read_from_client_enabled_) {
        cliSideSock_ev_->set_readcb(NULL);
        read_from_client_enabled_ = false;
    }
}

Handler::~Handler()
{
    if (active_fd_ != -1) {
        close(active_fd_);
        active_fd_ = -1;
    }

    if (cliSideSock_ev_) {
        cliSideSock_ev_->set_close_fd(false);
        delete cliSideSock_ev_;
        cliSideSock_ev_ = NULL;
    }
        
    if (cliSideSock_ != -1) {
        close(cliSideSock_);
        cliSideSock_ = -1;
    }

    if (outbuf_) {
        evbuffer_free(outbuf_);
        outbuf_ = NULL;
    }
    if (inbuf_) {
        evbuffer_free(inbuf_);
        inbuf_ = NULL;
    }

    /* don't delete evbase */
    evbase_ = NULL;
}

void
Handler::recv_from_client()
{
    logself(DEBUG, "begin");

    /* for some reason, evbuffer_read() fails on "bad file
     * descriptor". so we have to read(fd_) ourselves.
     *
     * use iov to reduce copying.
     */

    struct evbuffer_iovec v[2];
    int n = 0, i = 0, num_to_commit = 0;
    static const size_t n_to_add = 1024 * ARRAY_LEN(v);

    bool no_more_to_read = false; /* ... from the socket at this
                                   * time */

read_more:
    n = 0;
    i = 0;
    num_to_commit = 0;

    logself(DEBUG, "let's try to read from cliSideSock_");

    n = evbuffer_reserve_space(inbuf_, n_to_add, v, ARRAY_LEN(v));
    myassert(n>0);
    /* only need to call evbuffer_commit_space() if we will use the
     * data. in particular, if going to close connection, then no need
     * to commit. */

    for (i=0; i<n && n_to_add > 0; ++i) {
        size_t len = v[i].iov_len;
        if (len > n_to_add) {/* Don't write more than n_to_add bytes. */
            len = n_to_add;
        }
        const ssize_t numread = recv(cliSideSock_, v[i].iov_base, len, 0);
        if (numread == 0) {
            logself(DEBUG, "cnx is closed");
            /* since we only support GET requests, if the cnx has
             * closed, then we won't be able send response (assuming
             * that client is not doing half-closing, which is not
             * supported by shadow at this time). therefore, we just
             * clean up */
            on_client_sock_eof();
            goto done;
        } else if (numread == -1) {
            if (errno != EWOULDBLOCK) {
                logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
                      "recv() returned \"%s\"", strerror(errno));
                on_client_sock_error();
                goto done;
            }
        } else {
            myassert(numread > 0);
            logself(DEBUG, "able to read %zd bytes", numread);
            logself(DEBUG, "data: %s", v[i].iov_base);
            ++num_to_commit;
            /* Set iov_len to the number of bytes we actually wrote,
               so we don't commit too much. */
            v[i].iov_len = numread;
            if (numread < len) {
                // did not read as much as we wanted
                logself(DEBUG, "read less than wanted --> don't try to read "
                        "more until next readable event");
                no_more_to_read = true;
                break;
            } else {
                myassert(len == numread);
            }
        }
    }

    if (0 == num_to_commit) {
        logself(DEBUG, "not able to read anything/more -> return");
        /* XXX/TODO: should we disable read; otherwise, shadow will
         * repeatedly notify readable event and we waste cpu cycles?
         * if we do disable, when to re-enable? */
        goto done;
    }

    /* We commit the space here.  Note that we give it 'i' (the number
       of vectors we actually used) rather than 'n' (the number of
       vectors we had available. */
    if (evbuffer_commit_space(inbuf_, v, num_to_commit) < 0) {
        myassert(0);
    }

    logself(DEBUG, "num bytes available in inbuf: %d",
            evbuffer_get_length(inbuf_));

    if (process_inbuf_() && !no_more_to_read) {
        logself(DEBUG, "process_inbuf_() wants more data...");
        goto read_more;
    }

done:
    logself(DEBUG, "done");
}

bool
Handler::process_inbuf_()
{
    bool want_more_data = false;
    char *line = NULL;

    if (evbuffer_get_length(inbuf_) == 0)
        return true;

    line = evbuffer_readln(
        inbuf_, NULL, EVBUFFER_EOL_CRLF_STRICT);
    if (line)
        logself(DEBUG, "got request line: [%s]", line);
}


void
Handler::send_to_client()
{
}

void
Handler::deleteLater()
{
    logself(DEBUG, "begin, scheduling delayed freeing of Ox%X", this);
    scheduleCallback(&delete_handler, this, 0);
}

void
Handler::on_client_sock_error()
{
    logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
          "Client socket error. We are closing...");
    deleteLater();
}

void
Handler::on_client_sock_eof()
{
    logself(DEBUG, "client socket closed");
    deleteLater();
}

Handler::Handler(myevent_base* evbase, const string& docroot,
                 const int cliSideSock)
    : instNum_(nextInstNum), docroot_(docroot), evbase_(evbase)
    , cliSideSock_ev_(NULL), cliSideSock_(cliSideSock)
    , inbuf_(NULL), outbuf_(NULL)
    , active_fd_(-1)
    , peer_port_(0)
    , numRespBodyBytesExpectedToSend_(0), numBodyBytesRead_(0), numRespBytesSent_(0)
#ifdef TEST_BYTE_RANGE
    , numRespMetaBytes_(0)
#endif
{
    logself(DEBUG, "begin");
    ++nextInstNum;

    myassert(evbase_);

    cliSideSock_ev_ = new myevent_socket_t(
        evbase_, cliSideSock_, mev_readcb, NULL, mev_eventcb, this);
    myassert(cliSideSock_ev_);
    cliSideSock_ev_->set_logfn(logfn);
    cliSideSock_ev_->set_connected(); // sock's already connected
    myassert(0 == cliSideSock_ev_->start_monitoring());
    write_to_client_enabled_ = false;
    read_from_client_enabled_ = true;

    inbuf_ = evbuffer_new();
    outbuf_ = evbuffer_new();

    struct sockaddr_in peer_addr;
    socklen_t addr_len = sizeof (peer_addr);

    myassert(!getpeername(
                 cliSideSock_, (struct sockaddr*)&peer_addr, &addr_len));
    peer_port_ = peer_addr.sin_port;

    logself(DEBUG, "done");
}
