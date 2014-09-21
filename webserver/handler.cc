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

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include "http_parse.h"

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif

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

    if (MAX_PIPELINE_REQS == submitted_req_queue_.size()) {
        // disable reading
        logself(DEBUG, "reached max pipeline -> disable reading");
        disable_read_from_client_();
        goto done;
    }

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

    logself(DEBUG, "begin");

    if (evbuffer_get_length(inbuf_) == 0) {
        want_more_data = true;
        goto done;
    }

extract_request:
    line = NULL;
    switch (http_req_state_) {
    case HTTP_REQ_STATE_REQ_LINE: {
        /* readln() does drain the buffer */
        line = evbuffer_readln(
            inbuf_, NULL, EVBUFFER_EOL_CRLF_STRICT);
        if (line) {
            logself(DEBUG, "got request line: [%s]", line);

            /* get the (relative) path of the request */
            char* relpath = strcasestr(line, "GET ");
            myassert(relpath);
            relpath += 4;
            if ('/' != relpath[0]) {
                logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                      "URI path [%s] does not begin with a '/'");
                myassert(0);
            }

            char* relpath_end = strcasestr(relpath, " ");
            myassert(relpath_end);
            *relpath_end = '\0';

            submitted_req_queue_.push(RequestInfo(relpath));
            logself(DEBUG, "num requests in queue %u",
                    submitted_req_queue_.size());

#ifdef DEBUG_PIPELINE
            static int maxpipelinesize_seen = 1;
            /* log so we know the pipeline support is actually used */
            if (submitted_req_queue_.size() > maxpipelinesize_seen) {
                maxpipelinesize_seen = submitted_req_queue_.size();
                logfn(SHADOW_LOG_LEVEL_MESSAGE, __func__,
                      "maxpipelinesize_seen= %u", maxpipelinesize_seen);
            }
#endif
            /* be strict and crash here */
            myassert(MAX_PIPELINE_REQS >= submitted_req_queue_.size());

            http_req_state_ = HTTP_REQ_STATE_HEADERS;
            free(line);
            line = NULL;
            goto extract_request;
        }
        logself(DEBUG, "want more from socket");
        want_more_data = true;
        break;
    }
    case HTTP_REQ_STATE_HEADERS: {
        logself(DEBUG, "try to get request headers");
        while (NULL != (line = evbuffer_readln(
                            inbuf_, NULL, EVBUFFER_EOL_CRLF_STRICT)))
        {
            if (line[0] == '\0') {
                logself(DEBUG, "no more hdrs");
                http_req_state_ = HTTP_REQ_STATE_REQ_LINE;
                logself(DEBUG,
                        "done extracting a request -> quickly try to write");
                enable_write_to_client_();
                want_more_data = false;
                free(line);
                line = NULL;
                goto done;
            } else {
                logself(DEBUG, "whole req hdr line: [%s]", line);

                if (!strncasecmp(line, "Range: ", 7)) {
                    int first_byte_pos = -1;
                    int last_byte_pos = -1;
                    logself(MESSAGE, "line [%s]", line);
                    myassert(-1 != parseRange(line, 7, &first_byte_pos, &last_byte_pos));
                    myassert(last_byte_pos == -1); // not yet supporting last_byte_pos
                    myassert(first_byte_pos >= 0);

                    logself(DEBUG, "parsed first_byte_pos_ [%d]", first_byte_pos);
                    submitted_req_queue_.back().first_byte_pos = first_byte_pos;
                }

                free(line);
                line = NULL;
            }
        }
        logself(DEBUG, "want more from socket");
        want_more_data = true;
        break;
    }

    default:
        myassert(0);
        break;
    }

done:
    if (line) {
        free(line);
    }
    if (submitted_req_queue_.size()) {
        enable_write_to_client_();
    }
    if (want_more_data) {
        enable_read_from_client_();
    }
    logself(DEBUG, "done");
    return want_more_data;
}


void
Handler::send_to_client()
{
#define READ_HIGH_WATER_MARK (16*1024)
    /* this function both reads from the file into the outbuf and
     * drains from outbuf to write into the socket.
     *
     * if the outbuf is >= 16K, then will not try to read into any
     * more (until subsequently drained below 16K).
     *
     * on the other hand, if outbuf is >= 4K, then will prioritize
     * draining it to write into socket over reading from disk into
     * the buf.
     *
     */

    logself(DEBUG, "begin");

    /* set this when socket send/write() returns either EWOULDBLOCK or
     * an amount less than we asked it to.
     */
    bool send_would_block = false;

    /* we have circular goto's, loop, etc. so use this count to make
     * sure we're not stuck looping and making no progress
     */
    int count = 0;

    size_t running_num_written = 0;

add_more:
    ++count;

    /*
     * if we have written a total of 256 KB in this time around, we
     * call process_inbuf_().
     */
    if (running_num_written >= (256 * 1024)) {
        process_inbuf_();
        // reset
        running_num_written = 0;
#ifdef TEST_PIPELINE
        logself(DEBUG, "take a break from sending");
        goto done;
#endif
    }

    logself(DEBUG, "try to read from file into outbuf");
    myassert(count < 0x2f);
    if ((submitted_req_queue_.size() == 0)
        && (evbuffer_get_length(outbuf_) == 0))
    {
        disable_write_to_client_();
        process_inbuf_();
        goto done;
    }

    if ((evbuffer_get_length(outbuf_) >= (READ_HIGH_WATER_MARK))
        && send_would_block)
    {
        /* reached our buffer limit and send would block -> get out of
         * here */
        goto done;
    }
    
    while (submitted_req_queue_.size()
           && (evbuffer_get_length(outbuf_) < (READ_HIGH_WATER_MARK)))
    {
        // there's some request to be served. additionally we haven't
        // buffered too much
        count = 0;

//        if (evbuffer_get_length(outbuf_) > (4*1024) && !send_would_block) {
        if (evbuffer_get_length(outbuf_) > (1) && !send_would_block) {
            logself(
                DEBUG, "actually let's first send some data already in outbuf");
            goto send_more;
        }

        switch (http_rsp_state_) {
        case HTTP_RSP_STATE_META: {
            // serve the request at the front of queue
            const string abspath = docroot_ + submitted_req_queue_.front().path;
            const int& first_byte_pos = submitted_req_queue_.front().first_byte_pos;
            logself(DEBUG, "abs path: [%s], first_byte_pos %d",
                    abspath.c_str(), first_byte_pos);

            /* find out file size */
            struct stat sb;
            if (0 != stat(abspath.c_str(), &sb)) {
                logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                      "cannot access file [%s], errno str [%s]",
                      abspath.c_str(), strerror(errno));
                myassert(0);
            }

            myassert(sb.st_size > 0);

            if (! (first_byte_pos < sb.st_size)) {
                logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                      "invalid first_byte_pos %d for %s; file size is %zu.",
                      first_byte_pos, abspath.c_str(), sb.st_size);
                myassert(0);
            }

            size_t content_length = sb.st_size;
            int last_byte_pos = -1;
            uint32_t resp_status = 200;

            if (first_byte_pos >= 0) {
                resp_status = 206;
                content_length -= first_byte_pos;
                last_byte_pos = first_byte_pos + content_length - 1;
                myassert(last_byte_pos == (sb.st_size - 1));
                myassert(last_byte_pos >= first_byte_pos);
            }

            /* determine content type */
            const char *content_type = NULL;
            const char *abspath_cstr = abspath.c_str(); 
            const char *dot = strrchr(abspath_cstr, '.');
            if (!dot || dot == abspath_cstr) {
                content_type = "unknown";
            } else {
                ++dot;
                if (!strcmp(dot, "html")) {
                    content_type = "text/html";
                } else {
                    content_type = "unknown";
                }
            }

            logself(DEBUG, "content len [%zu], type [%s]",
                    content_length, content_type);

            int r = evbuffer_add_printf(
                outbuf_,
                "HTTP/1.1 %u OK\r\nContent-Length: %ld\r\nContent-Type: %s\r\n",
                resp_status, content_length, content_type);
            myassert(0 < r);
#ifdef TEST_BYTE_RANGE
            numRespMetaBytes_ += r;
#endif

            if (first_byte_pos >= 0) {

#ifdef TEST_BYTE_RANGE
                const int m = (rand() % 3);
                int bad_first_byte_pos = first_byte_pos;
                if (m == 0) {
                    bad_first_byte_pos += 1;
                } else if (m == 1) {
                    bad_first_byte_pos -= 1;
                }
                r = evbuffer_add_printf(
                    outbuf_,
                    "Content-Range: bytes %d-%d/%zu\r\n",
                    bad_first_byte_pos, last_byte_pos, sb.st_size);
#else
                r = evbuffer_add_printf(
                    outbuf_,
                    "Content-Range: bytes %d-%d/%zu\r\n",
                    first_byte_pos, last_byte_pos, sb.st_size);
#endif
                myassert(0 < r);

#ifdef TEST_BYTE_RANGE
                numRespMetaBytes_ += r;
#endif
            }

            r = evbuffer_add_printf(outbuf_, "\r\n");
            myassert(0 < r);
#ifdef TEST_BYTE_RANGE
            numRespMetaBytes_ += r;
#endif

            http_rsp_state_ = HTTP_RSP_STATE_BODY;

            myassert(-1 == active_fd_);
            active_fd_ = open(abspath.c_str(), O_RDONLY);
            myassert(-1 != active_fd_);

            if (first_byte_pos > 0) {
                myassert(
                    first_byte_pos == lseek(active_fd_, first_byte_pos, SEEK_SET));
            }

            numRespBodyBytesExpectedToSend_ = content_length;
            numBodyBytesRead_ = 0;

            if (!send_would_block) {
                /* want to get the meta info out quick */
                goto send_more;
            }
        }
        case HTTP_RSP_STATE_BODY: {
            struct evbuffer_iovec v[2];
            int n = 0, i = 0, num_to_commit = 0;
            static const size_t n_to_add = 4096 * ARRAY_LEN(v);
            bool done_with_current_file = false;

            n = evbuffer_reserve_space(outbuf_, n_to_add, v, ARRAY_LEN(v));
            myassert(n>0);

            for (i=0; i<n && n_to_add > 0; ++i) {
                size_t len = v[i].iov_len;
                if (len > n_to_add) {/* Don't write more than n_to_add bytes. */
                    len = n_to_add;
                }
                const int numread = read(active_fd_, v[i].iov_base, len);
                if (numread == 0) {
                    logself(DEBUG, "reached end-of-file");
                    done_with_current_file = true;
                    break;
                } else if (numread == -1) {
                    logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                          "error reading [%s]: \"%s\"",
                          submitted_req_queue_.front().path.c_str(), strerror(errno));
                    myassert(0);
                } else {
                    myassert(numread > 0);
                    logself(DEBUG, "read %zd bytes", numread);
                    numBodyBytesRead_ += numread;
                    logself(DEBUG, "new numBodyBytesRead_ %zu",
                            numBodyBytesRead_);
                    ++num_to_commit;
                    /* Set iov_len to the number of bytes we actually wrote,
                       so we don't commit too much. */
                    v[i].iov_len = numread;
                    if (numread < len) {
                        logself(DEBUG, "read less than wanted: done with file");
                        myassert(numRespBodyBytesExpectedToSend_ == numBodyBytesRead_);
                        done_with_current_file = true;
                        break;
                    } else {
                        myassert(len == numread);
                    }
                }
            }

            if (num_to_commit) {
                /* We commit the space here. */
                if (evbuffer_commit_space(outbuf_, v, num_to_commit) < 0) {
                    myassert(0);
                }
            }
            logself(DEBUG, "num bytes available in outbuf: %d",
                    evbuffer_get_length(outbuf_));

            if (done_with_current_file) {
                logself(DEBUG, "done processing req for [%s]",
                        submitted_req_queue_.front().path.c_str());
                close(active_fd_);
                active_fd_ = -1;
                numRespBytesSent_ = numBodyBytesRead_ = numRespBodyBytesExpectedToSend_ = 0;
#ifdef TEST_BYTE_RANGE
                numRespMetaBytes_ = 0;
#endif
                submitted_req_queue_.pop();
                logself(DEBUG, "new qsize %u", submitted_req_queue_.size());
                http_rsp_state_ = HTTP_RSP_STATE_META;
                /* we just opened up a spot on the queue, so start
                 * processing/reading again
                 */
                if (submitted_req_queue_.size() == 0) {
                    /* XXX/ dont know why we disabled writing here!?
                     * we are only done with reading the file, not
                     * with sending it.
                     */
                    // disable_write_to_client_();
                }
                process_inbuf_();
            }
        }
        }
    }

send_more:
    ++count;

    logself(DEBUG, "try to send into socket");

    while (evbuffer_get_length(outbuf_) > 0 && !send_would_block) {
        count = 0;
        struct evbuffer_iovec v[2];
        int numdrained = 0;
        const int n = evbuffer_peek(outbuf_, -1, NULL, v, ARRAY_LEN(v));

        for (int i = 0; i < n; ++i) {
            const int numwritten = write(
                cliSideSock_, (const uint8_t *)v[i].iov_base, v[i].iov_len);
            if (numwritten == -1) {
                if (errno == EWOULDBLOCK) {
                    send_would_block = true;
                    logself(DEBUG, "send_would_block: %u", send_would_block);
                } else {
                    logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                          "error reading [%s]: \"%s\"",
                          submitted_req_queue_.front().path.c_str(),
                          strerror(errno));
                    on_client_sock_error();
                    goto done;
                }
                break;
            } else {
                logself(DEBUG, "able to write %d bytes", numwritten);
                numdrained += numwritten;
                numRespBytesSent_ += numwritten;
                logself(DEBUG, "new numRespBytesSent_ %zu", numRespBytesSent_);

#ifdef TEST_BYTE_RANGE
                // fake an error, and we expect browser to retry to
                // get the object with updated byte range.
                if ((numRespBytesSent_ - numRespMetaBytes_) > 36131) {
                    if ((rand() % 2) == 0) {
                        logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
                              "fake error. close after sending %d bytes",
                              (numRespBytesSent_ - numRespMetaBytes_));
                        close(cliSideSock_);
                        on_client_sock_eof();
                        return;
                    }
                }
#endif

                running_num_written += numwritten;
                if (numwritten != v[i].iov_len) {
                    logself(DEBUG, "couldn't write the whole iov -> move on");
                    send_would_block = true; /* might not be accurate,
                                              * but it won't cause
                                              * correctness issue*/
                    break;
                }
            }
        }
        logself(DEBUG, "drained total of %d bytes", numdrained);
        myassert(0 == evbuffer_drain(outbuf_, numdrained));
    }

    if (submitted_req_queue_.size()) {
        logself(DEBUG, "still some request left -> try to read more");
        goto add_more;
    }

done:
    logself(DEBUG, "done");
#undef READ_HIGH_WATER_MARK
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
    , http_req_state_(HTTP_REQ_STATE_REQ_LINE)
    , http_rsp_state_(HTTP_RSP_STATE_META)
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
