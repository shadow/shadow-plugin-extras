#include "connection.hpp"

#include <errno.h>
#include <netinet/tcp.h>
#include <string.h>

#include <algorithm>
#include <vector>

using std::vector;
using std::string;
using std::pair;

#include "common.hpp"

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include "http_parse.h"

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif


#ifdef ENABLE_MY_LOG_MACROS
/* "inst" stands for instance, as in, instance of a class */
#define loginst(level, inst, fmt, ...)                                  \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, cnx= %d): " fmt,  \
              __LINE__, (inst)->instNum_, ##__VA_ARGS__);               \
    } while (0)

/* like loginst, but shortcut "this" as instance */
#define logself(level, fmt, ...)                                        \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, cnx= %d): " fmt,  \
              __LINE__, (this)->instNum_, ##__VA_ARGS__);               \
    } while (0)
#else

/* no op */
#define loginst(level, inst, fmt, ...)

/* no op */
#define logself(level, fmt, ...)

#endif

uint32_t Connection::nextInstNum = 0;

extern ShadowLogFunc logfn;

//namespace {
static void
mev_readcb(int fd, void *ptr)
{
    int rv;
    logDEBUG("begin");
    Connection *conn = reinterpret_cast<Connection*>(ptr);
    conn->on_read();
    logDEBUG("done");
}
//} // namespace

//namespace {
static void
mev_writecb(int fd, void *ptr)
{
    int rv;
    Connection *conn = reinterpret_cast<Connection*>(ptr);
    loginst(DEBUG, conn, "begin");
    conn->on_write();
    loginst(DEBUG, conn, "done");
}
//} // namespace

//namespace {
static void
mev_eventcb(int fd, short events, void *ptr)
{
    Connection *conn = reinterpret_cast<Connection*>(ptr);
    if(events & MEV_EVENT_CONNECTED) {
        loginst(DEBUG, conn, "Connection established");
        conn->on_connect();
        /* not calling setsockopt( tcp_nodelay) because shadow doesn't
         * implement it */
    } else if(events & MEV_EVENT_EOF) {
        loginst(DEBUG, conn, "EOF");
        conn->on_eof();
    } else if(events & (MEV_EVENT_ERROR)) {
        loginst(WARNING, conn, "Network error");
        conn->on_error();
    }
}
//} // namespace

static ssize_t
spdylay_send_cb(spdylay_session *session, const uint8_t *data,
                size_t length, int flags, void *user_data)
{
    Connection *conn = reinterpret_cast<Connection*>(user_data);
    return conn->spdylay_send_cb(session, data, length, flags);;
}

static ssize_t
spdylay_recv_cb(spdylay_session *session, uint8_t *buf, size_t length,
                int flags, void *user_data)
{
    Connection *conn = reinterpret_cast<Connection*>(user_data);
    return conn->spdylay_recv_cb(session, buf, length, flags);
}

static void
spdylay_on_data_recv_cb(spdylay_session *session,
                        uint8_t flags, int32_t stream_id,
                        int32_t len, void *user_data)
{
    Connection *conn = reinterpret_cast<Connection*>(user_data);
    conn->spdylay_on_data_recv_cb(session, flags, stream_id, len);
}

static void
spdylay_on_data_chunk_recv_cb(spdylay_session *session,
                              uint8_t flags, int32_t stream_id,
                              const uint8_t *data, size_t len,
                              void *user_data)
{
    Connection *conn = reinterpret_cast<Connection*>(user_data);
    conn->spdylay_on_data_chunk_recv_cb(session, flags, stream_id,
                                        data, len);
}

static void
spdylay_on_ctrl_recv_cb(spdylay_session *session, spdylay_frame_type type,
                        spdylay_frame *frame, void *user_data)
{
    Connection *conn = reinterpret_cast<Connection*>(user_data);
    conn->spdylay_on_ctrl_recv_cb(session, type, frame);
}

//namespace {
void
spdylay_before_ctrl_send_cb(spdylay_session *session,
                            spdylay_frame_type type,
                            spdylay_frame *frame,
                            void *user_data)
{
    Connection *conn = reinterpret_cast<Connection*>(user_data);
    conn->spdylay_before_ctrl_send_cb(session, type, frame);
}
//} // namespace

static void
set_up_spdylay_session(spdylay_session **session_, void* user_data)
{
    int r;
    spdylay_session_callbacks callbacks;
    bzero(&callbacks, sizeof callbacks);
    callbacks.send_callback = spdylay_send_cb;
    callbacks.recv_callback = spdylay_recv_cb;
    callbacks.on_ctrl_recv_callback = spdylay_on_ctrl_recv_cb;
    callbacks.on_data_chunk_recv_callback = spdylay_on_data_chunk_recv_cb;
    callbacks.on_data_recv_callback = spdylay_on_data_recv_cb;
    /* callbacks.on_stream_close_callback = spdylay_on_stream_close_cb; */
    callbacks.before_ctrl_send_callback = spdylay_before_ctrl_send_cb;
    /* callbacks.on_ctrl_not_send_callback = spdylay_on_ctrl_not_send_cb; */

    // version 3 just adds flow control, compared to version 2
    r = spdylay_session_client_new(session_, 2, &callbacks, user_data);
    myassert(0 == r);

#if 0
    spdylay_settings_entry entry[2];
    entry[0].settings_id = SPDYLAY_SETTINGS_MAX_CONCURRENT_STREAMS;
    entry[0].value = 8; // XXX
    entry[0].flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;

    entry[1].settings_id = SPDYLAY_SETTINGS_INITIAL_WINDOW_SIZE;
    entry[1].value = 4096; // XXX
    entry[1].flags = SPDYLAY_ID_FLAG_SETTINGS_NONE;

    r = spdylay_submit_settings(
        *session_, SPDYLAY_FLAG_SETTINGS_NONE,
        entry, sizeof(entry)/sizeof(spdylay_settings_entry));
    myassert (0 == r);
#endif
}

void
Connection::http_write_to_outbuf()
{
    logself(DEBUG, "begin");
    if (submitted_req_queue_.size() == 0) {
        logself(DEBUG, "submit queue is empty");
        return;
    }
    Request *req = NULL;
    const int qsize = active_req_queue_.size();
    logself(DEBUG, "active req qsize %u", qsize);
    if (!do_pipeline_) {
        if (qsize > 0) {
            logself(DEBUG, "no pipeline, and there's an active req -> wait");
            return;
        }
        // else fall through to send req
    } else {
        // doing pipeline, but limit num of concurrent reqs
        if (qsize >= max_pipeline_size_) {
            logself(DEBUG,
                    "already %u reqs in active pipeline -> wait", qsize);
            return;
        }
        // else fall through to send req
    }
    logself(DEBUG, "writing a req to outbuf");
    req = submitted_req_queue_.front();
    myassert(req);
    submitted_req_queue_.pop_front();

    myassert(0 < evbuffer_add_printf(
               outbuf_, "GET %s HTTP/1.1\r\n", req->path_.c_str()));
    const vector<pair<string, string> >& hdrs = req->get_headers();
    vector<pair<string, string> >::const_iterator it = hdrs.begin();
    for (; it != hdrs.end(); ++it) {
        myassert(0 < evbuffer_add_printf(
                   outbuf_, "%s: %s\r\n", it->first.c_str(),
                   it->second.c_str()));
    }

    first_byte_pos_ = req->get_first_byte_pos();
    logself(DEBUG, "adding first_byte_pos_ %zu", first_byte_pos_);
    myassert(0 < evbuffer_add_printf(
                 outbuf_, "Range: bytes=%zu-\r\n", first_byte_pos_));

    myassert(2 == evbuffer_add_printf(outbuf_, "\r\n"));
    active_req_queue_.push(req);

    if (state_ == CONNECTED) {
        /* we might not be fully connected yet, e.g., still
         * negotiating with the socks proxy, in which case we don't
         * want to interfere.
         */
        enable_write_to_server_();
    }
    
    logself(DEBUG, "done");
    return;
}

int
Connection::submit_request(Request* req)
{
    /* it's ok to submit requests if not yet connected, etc, but proly
     * a bug if submitting a request after we have closed
     */
    myassert((state_ != NO_LONGER_USABLE) && (state_ != DESTROYED));

    logself(DEBUG, "begin");
    logself(DEBUG, "path [%s]", req->get_path().c_str());
    if (use_spdy_) {
        const vector<pair<string, string> >& hdrs = req->get_headers();
        const char **nv = (const char**)calloc(5*2 + hdrs.size()*2 + 1, sizeof(char*));
        size_t hdidx = 0;
        nv[hdidx++] = ":method";
        nv[hdidx++] = "GET";
        nv[hdidx++] = ":path";
        nv[hdidx++] = req->get_path().c_str();
        nv[hdidx++] = ":version";
        nv[hdidx++] = "HTTP/1.1";
        nv[hdidx++] = ":host";
        nv[hdidx++] = req->host_.c_str();
        nv[hdidx++] = ":scheme";
        nv[hdidx++] = "http";
        vector<pair<string, string> >::const_iterator it = hdrs.begin();
        for (; it != hdrs.end(); ++it) {
            const char *name = it->first.c_str();
            const char *value = it->second.c_str();
            logself(DEBUG, "hdr name [%s] value [%s]", name, value);
            nv[hdidx++] = name;
            nv[hdidx++] = value;
        };
        nv[hdidx++] = NULL;

        /* spdylay_submit_request() will make copies of nv */
        int rv = spdylay_submit_request(spdysess_, 0, nv, NULL, req);
        myassert(rv == 0);
        free(nv);
        enable_write_to_server_();
    } else {
        submitted_req_queue_.push_back(req);
        /* http_write_to_outbuf() takes care of enabling the write
         * event. */
        http_write_to_outbuf();
    }

    req->conn = this;

    logself(DEBUG, "done");
    return 0;
}

void
Connection::enable_write_to_server_()
{
    if (!write_to_server_enabled_) {
        ev_->set_writecb(mev_writecb);
        write_to_server_enabled_ = true;
    } else {
        logself(DEBUG, "no-op because was already enabled");
    }
}

void
Connection::disable_write_to_server_()
{
    if (write_to_server_enabled_) {
        ev_->set_writecb(NULL);
        write_to_server_enabled_ = false;
    } else {
        logself(DEBUG, "no-op because was already disabled");
    }
}

//namespace {
void
mev_socks5_proxy_readcb(int fd, void *ptr)
{
    Connection *conn = reinterpret_cast<Connection*>(ptr);
    conn->on_socks5_proxy_readable();
}

void
mev_socks5_proxy_writecb(int fd, void *ptr)
{
    Connection *conn = reinterpret_cast<Connection*>(ptr);
    conn->on_socks5_proxy_writable();
}
//} // namespace

//namespace {
static void
mev_socks5_proxy_eventcb(int fd, short events, void *ptr)
{
    Connection *conn = reinterpret_cast<Connection*>(ptr);
    loginst(DEBUG, conn, "begin");
    if(events & MEV_EVENT_CONNECTED) {
        loginst(DEBUG, conn, "Connected to the socks5 proxy");
        conn->on_socks5_proxy_connected();
    } else if(events & MEV_EVENT_EOF) {
        myassert(0);
    } else if(events & (MEV_EVENT_ERROR)) {
        myassert(0);
    }
    loginst(DEBUG, conn, "done");
}
//} // namespace

Connection::Connection(
    myevent_base *evbase,
    const in_addr_t& addr, const in_port_t& port,
    const in_addr_t& socks5_addr, const in_port_t& socks5_port,
    const in_addr_t& ssp_addr, const in_port_t& ssp_port,
    ConnectionErrorCb error_cb, ConnectionEOFCb eof_cb,
    PushedMetaCb pushed_meta_cb, PushedBodyDataCb pushed_body_data_cb,
    PushedBodyDoneCb pushed_body_done_cb,
    void *cb_data, const bool& use_spdy
    )
    : instNum_(nextInstNum), use_spdy_(use_spdy),evbase_(evbase), ev_(NULL), fd_(-1)
    , state_(DISCONNECTED), socks5_state_(SOCKS5_NONE)
    , addr_(addr), port_(port)
    , socks5_addr_(socks5_addr), socks5_port_(socks5_port)
    , ssp_addr_(ssp_addr), ssp_port_(ssp_port)
    , cnx_error_cb_(error_cb), cnx_eof_cb_(eof_cb)
    , notify_pushed_meta_(pushed_meta_cb)
    , notify_pushed_body_data_(pushed_body_data_cb)
    , notify_pushed_body_done_(pushed_body_done_cb)
    , spdysess_(NULL), inbuf_(NULL), outbuf_(NULL), do_pipeline_(false)
    , max_pipeline_size_(1)
    , http_rsp_state_(HTTP_RSP_STATE_STATUS_LINE)
    , http_rsp_status_(-1), first_byte_pos_(0), body_len_(-1)
    , cumulative_num_sent_bytes_(0), cumulative_num_recv_bytes_(0)
    , write_to_server_enabled_(false)
{
    ++nextInstNum;

    /* ssp acts as an http proxy, only it uses spdy to transport. so
     * if ssp is used, then we don't need the actual address of the
     * final site, as the ":host" header will take care of
     * that. otherwise, i.e., no ssp host is specified, then we need
     * to the final site's address.
     */
    if (ssp_addr_) {
        myassert(!addr_);
    } else {
        myassert(addr_);
    }

    if (use_spdy_) {
        /* it's ok to set up the session now even though socket is not
         * connected */
        set_up_spdylay_session(&spdysess_, this);
    } else {
        inbuf_ = evbuffer_new();
        outbuf_ = evbuffer_new();
    }

    initiate_connection();
}

Connection::~Connection()
{
    logself(DEBUG, "begin destructor");
    disconnect();
    evbase_ = NULL; // no freeing
    logself(DEBUG, "done destructor");
}

std::queue<Request*>
Connection::get_active_request_queue() const
{
    return active_req_queue_;
}

std::deque<Request*>
Connection::get_pending_request_queue() const
{
    return submitted_req_queue_;
}

void
Connection::set_request_done_cb(ConnectionRequestDoneCb cb)
{
    notify_request_done_cb_ = cb;
}

static void
delete_cnx(void *ptr)
{
    delete ((Connection*)ptr);
}

void
Connection::deleteLater(ShadowCreateCallbackFunc scheduleCallback)
{
    scheduleCallback(delete_cnx, this, 0);
}

bool
Connection::is_idle() const {
    if (state_ == CONNECTED) {
        if (use_spdy_) {
            return (!spdylay_session_want_read(spdysess_)
                    && !spdylay_session_want_write(spdysess_));
        } else {
            const int subqsize = submitted_req_queue_.size();
            const int activeqsize = active_req_queue_.size();
            logself(DEBUG, "%d, %d", subqsize, activeqsize);
            return (submitted_req_queue_.size() == 0
                    && active_req_queue_.size() == 0);
        }
    }
    return false;
}

void
Connection::disconnect()
{
    logself(DEBUG, "begin");
    // myassert(0 == spdylay_submit_goaway(spdysess_, 0));
    // send_();

    /* Connection::spdylay_recv_cb() is failling the session ==
     * spdysess_ assert during some cleanup. let's try to kill the
     * event first, so it doesn't continue to notify
     * on_read(). UPDATE: doesn't help, so i'll just disable the
     * assert, and simply do nothing if the check fails.
     */
    if (ev_) {
        ev_->set_close_fd(false);
        delete ev_;
        ev_ = NULL;
    }
    if(fd_ != -1) {
        logself(DEBUG, "closing fd %d", fd_);
        //shutdown(fd_, SHUT_WR); // shadow doesn't support shutdown()
        close(fd_);
        fd_ = -1;
    }

    if (spdysess_) {
        spdylay_session_del(spdysess_);
        spdysess_ = 0;
    }

    if (outbuf_) {
        evbuffer_free(outbuf_);
        outbuf_ = NULL;
    }
    if (inbuf_) {
        evbuffer_free(inbuf_);
        inbuf_ = NULL;
    }

    state_ = NO_LONGER_USABLE;
    logself(DEBUG, "done");
}

void
Connection::on_socks5_proxy_connected()
{
    Connection *conn = this;
    loginst(DEBUG, conn, "begin");
    // write the client greeting
    static const char req[] = "\x05\x01\x00";
    // don't use "sizeof req", which gives you 4
    myassert(write(fd_, req, 3) == 3);
    conn->socks5_state_ = SOCKS5_GREETING;
    // when the proxy replies, we will handle in socks5_proxy_readcb()
    ev_->set_readcb(mev_socks5_proxy_readcb);
    ev_->set_writecb(NULL);
    loginst(DEBUG, conn, "done");
}
    
void
Connection::on_connect()
{
    myassert(state_ == CONNECTING);
    logself(DEBUG, "now connected");
    state_ = CONNECTED;
    send_();
    logself(DEBUG, "done");
}

void
Connection::on_write()
{
    send_();
}

bool
Connection::http_receive()
{
    logself(DEBUG, "begin");
    bool reached_eof = false;

    if (active_req_queue_.size() == 0) {
        logself(DEBUG, "no active req waiting to be received");
        //disable read monitoring
        ev_->set_readcb(NULL);
        return reached_eof;
    }
    /* read into buffer */

    /* for some reason, evbuffer_read() fails on "bad file
     * descriptor". so we have to read(fd_) ourselves.
     *
     * use iov to reduce copying.
     */
    //int numread = evbuffer_read(inbuf_, fd_, -1);

    struct evbuffer_iovec v[2];
    int n = 0, i = 0, num_to_commit = 0;
    static const size_t n_to_add = 4096 * ARRAY_LEN(v);
    char *line = NULL;
    bool content_range_found = false;

read_more:
    n = 0;
    i = 0;
    num_to_commit = 0;

    n = evbuffer_reserve_space(inbuf_, n_to_add, v, ARRAY_LEN(v));
    myassert(n>0);

    for (i=0; i<n && n_to_add > 0; ++i) {
        size_t len = v[i].iov_len;
        if (len > n_to_add) {/* Don't write more than n_to_add bytes. */
            len = n_to_add;
        }
        const int numread = recv(fd_, v[i].iov_base, len, 0);
        if (numread == 0) {
            logself(DEBUG, "cnx is closed");
            reached_eof = true;
            break;
        } else if (numread == -1) {
            myassert(errno == EWOULDBLOCK);
        } else {
            myassert(numread > 0);
            if (0 == cumulative_num_recv_bytes_
                && cnx_first_recv_byte_cb_)
            {
                cnx_first_recv_byte_cb_(this);
            }
            cumulative_num_recv_bytes_ += numread;
            logself(DEBUG, "able to read %zd bytes", numread);
            ++num_to_commit;
            /* Set iov_len to the number of bytes we actually wrote,
               so we don't commit too much. */
            v[i].iov_len = numread;
            if (numread < len) {
                // did not read as much as we wanted --> assume no
                // more available now --> stop reading
                break;
            } else {
                myassert(len == numread);
            }
        }
    }

    if (0 == num_to_commit) {
        logself(DEBUG, "not able to read anything/more -> return");
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

    /* process it */
handle_response:
    line = NULL;
    switch (http_rsp_state_) {
    case HTTP_RSP_STATE_STATUS_LINE: {
        /* readln() does drain the buffer */
        line = evbuffer_readln(
            inbuf_, NULL, EVBUFFER_EOL_CRLF_STRICT);
        if (line) {
            logself(DEBUG, "got status line: [%s]", line);
            const char *tmp = strchr(line, ' ');
            myassert(tmp);
            http_rsp_status_ = strtol(tmp + 1, NULL, 10);
            if (http_rsp_status_ != 200 && http_rsp_status_ != 206) {
                Request *req = active_req_queue_.front();
                logfn(SHADOW_LOG_LEVEL_WARNING,
                      "req [%s] got status [%d]", req->url_.c_str(), 
                      http_rsp_status_);
                myassert(0);
            }
            logself(DEBUG, "status: [%d]", http_rsp_status_);
            http_rsp_state_ = HTTP_RSP_STATE_HEADERS;
            content_range_found = false;
            free(line);
            line = NULL;
            goto handle_response;
        }
        /* we read only n_to_add bytes from socket. more might be
         * available, so go try more.
         */
        goto read_more;
        break;
    }
    case HTTP_RSP_STATE_HEADERS: {
        logself(DEBUG, "try to get response headers");
        while (NULL != (line = evbuffer_readln(
                            inbuf_, NULL, EVBUFFER_EOL_CRLF_STRICT)))
        {
            /* a header line is assumped to be:

               <name>: <value>

               the "line" returned by evbuffer_readln() needs to be
               freed, but we avoid allocating more memory by having
               our name and value pointers point to the same
               "line". so when we free, only free the name pointers.
             */
            if (line[0] == '\0') {
                // no more hdrs
                myassert(body_len_ >= 0);
                myassert(0 == (rsp_hdrs_.size() % 2));

                if (!content_range_found) {
                    logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                          "response is missing content-range header.");
                    myassert(0);
                }

                char **nv = (char**)malloc((rsp_hdrs_.size() + 1) * sizeof (char*));
                int j = 0;
                for (; j < rsp_hdrs_.size(); ++j) {
                    nv[j] = rsp_hdrs_[j];
                    logself(DEBUG, "nv[%d] = [%s]", j, nv[j]);
                }
                nv[j] = NULL; /* null sentinel */
                Request *req = active_req_queue_.front();
                req->notify_rsp_meta(http_rsp_status_, nv);
                free(nv);
                for (int i = 0; i < rsp_hdrs_.size(); i += 2) {
                    free(rsp_hdrs_[i]);
                }
                rsp_hdrs_.clear();
                http_rsp_state_ = HTTP_RSP_STATE_BODY;
                free(line);
                line = NULL;
                goto handle_response;
            } else {
                logself(DEBUG, "whole rsp hdr line: [%s]", line);
                // XXX/TODO: expect all lower case
                char *tmp = strchr(line, ':');
                myassert(tmp);
                rsp_hdrs_.push_back(line);
                *tmp = '\0'; /* colon becomes NULL */
                ++tmp;
                *tmp = '\0'; /* blank space becomes NULL */
                ++tmp;
                rsp_hdrs_.push_back(tmp);
                if (!strcasecmp(line, "content-length")) {
                    body_len_ = strtol(tmp, NULL, 10);
                    logself(DEBUG, "body content length: [%d]", body_len_);
                } else if (!strcasecmp(line, "max-pipeline")) {
#if 0
                    max_pipeline_size_ = strtol(tmp, NULL, 10);
                    logself(DEBUG, "max pipeline size supported: [%u]", max_pipeline_size_);
#endif
                } else if (!strcasecmp(line, "content-range")) {
                    int first_byte_pos = 0;
                    int last_byte_pos = 0;
                    int full_len = 0;
                    myassert(-1 != parseContentRange(tmp, 0, &first_byte_pos, &last_byte_pos, &full_len));
                    if (first_byte_pos != first_byte_pos_) {
                        logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                              "response first-byte-pos is %d, but we expect %d", 
                              first_byte_pos, first_byte_pos_);
                        myassert(0);
                    }
                    logself(DEBUG, "parsed first_byte_pos %d, last_byte_pos %d, full_len %d",
                            first_byte_pos, last_byte_pos, full_len);
                    myassert((full_len - 1) == last_byte_pos);
                    content_range_found = true;
                }
                // DO NOT free line. because it's in the rsp_hdrs_;
            }
        }
        /* we read only n_to_add bytes from socket. more might be
         * available, so go try more.
         */
        goto read_more;
        break;
    }

    case HTTP_RSP_STATE_BODY: {
        myassert(body_len_ > 0);
        logself(DEBUG, "get rsp body, current body_len_ %d", body_len_);
        Request *req = active_req_queue_.front();
        while (evbuffer_get_length(inbuf_) > 0 && body_len_ > 0) {
            int numconsumed = 0;
            struct evbuffer_iovec v[2];
            const int n = evbuffer_peek(
                inbuf_, body_len_, NULL, v, ARRAY_LEN(v));
            for (int i = 0; i < n; ++i) {
                /* this iov might be more than what we asked for */
                /* at time point, body_len_ is guaranteed to be
                 * non-negative, so ok to cast to size_t
                 */
                const int consumed_of_this_one = std::min((size_t)body_len_, v[i].iov_len);
                numconsumed += consumed_of_this_one;
                body_len_ -= consumed_of_this_one;
                myassert(body_len_ >= 0);
                req->notify_rsp_body_data(
                    (const uint8_t *)v[i].iov_base, consumed_of_this_one);
            }
            logself(DEBUG, "consumed %d bytes -> new body_len_ %d",
                    numconsumed, body_len_);
            myassert(0 == evbuffer_drain(inbuf_, numconsumed));
        }
        if (body_len_ == 0) {
            /* remove req from active queue */
            active_req_queue_.pop();
            req->notify_rsp_body_done();
            body_len_ = -1;
            http_rsp_state_ = HTTP_RSP_STATE_STATUS_LINE;
            if (!notify_request_done_cb_.empty()) {
                notify_request_done_cb_(this, req);
            }
            /* if there's more in the submitted queue, we should be
             * able move some into the active queue, now that we just
             * cleared some space in the active queue
             */
            /* http_write_to_outbuf() takes care of enabling the
             * write event */
            http_write_to_outbuf();
            goto handle_response;
        }

        /* we read only n_to_add bytes from socket. more might be
         * available, so go try more.
         */
        goto read_more;
        break;
    }
    default:
        break;
    }

done:
    logself(DEBUG, "done");
    return !reached_eof;
}

void
Connection::on_read()
{
    int rv = 0;
    logself(DEBUG, "begin");
    if (use_spdy_) {
        spdylay_session* session_ = this->spdysess_;
        if((rv = spdylay_session_recv(session_)) != 0) {
            disconnect();
            if (SPDYLAY_ERR_EOF == rv) {
                logself(DEBUG, "remote peer closed");
                on_eof();
            } else {
                logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
                      "spdylay_session_recv() returned \"%s\"",
                      spdylay_strerror(rv));
                on_error();
            }
            return;
        } else if((rv = spdylay_session_send(session_)) < 0) {
            myassert(0);
        }
        if(rv == 0) {
            if(spdylay_session_want_read(session_) == 0 &&
               spdylay_session_want_write(session_) == 0) {
                myassert(0);
                rv = -1;
            }
        }
    } else {
        if (!http_receive()) {
            on_eof();
        }
    }
    logself(DEBUG, "done");
}

void
Connection::on_eof()
{
//    disconnect(); // let the user delete us
    cnx_eof_cb_(this);
}

void
Connection::on_error()
{
//    disconnect(); // let the user delete us
    cnx_error_cb_(this);
}

int
Connection::initiate_connection()
{
    logself(DEBUG, "begin");

    int rv = 0;

    logself(DEBUG, "socks5 %d:%d", socks5_addr_, socks5_port_);
    
    if (state_ == DISCONNECTED && socks5_addr_ && socks5_port_) {
        logself(DEBUG, "currently disconnected, and socks5 is specified "
                 "-> connect to socks5 proxy");
        /* connect to socks5 proxy */
        myassert(fd_ == -1);
        fd_ = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
        myassert(fd_ != -1);

        struct sockaddr_in server;
        bzero(&server, sizeof(server));
        server.sin_family = AF_INET;
        server.sin_addr.s_addr = socks5_addr_;
        server.sin_port = htons(socks5_port_);
        logself(DEBUG, " --> connecting sock proxy in_addr_t %u:%u",
                server.sin_addr.s_addr, server.sin_port);

        myassert(!ev_);
        // No need to set writecb because we write the request when
        // connected at once.
        ev_ = new myevent_socket_t(
            evbase_, fd_, NULL, mev_socks5_proxy_writecb,
            mev_socks5_proxy_eventcb, this);
        myassert(ev_);
        ev_->set_logfn(logfn);

        rv = ev_->socket_connect((struct sockaddr*)(&server), sizeof(server));
        myassert (!rv || errno == EINPROGRESS);

        logself(DEBUG, "transition to state CONNECTING");
        state_ = CONNECTING;
    }
    else if(state_ == DISCONNECTED) {
        logself(DEBUG, "cur state DISCONNECTED");
        // go ahead and connect
        myassert(fd_ == -1);
        fd_ = socket(AF_INET, (SOCK_STREAM | SOCK_NONBLOCK), 0);
        myassert(fd_ != -1);

        struct sockaddr_in server;
        bzero(&server, sizeof(server));
        server.sin_family = AF_INET;
        if (addr_) {
            server.sin_addr.s_addr = addr_;
            server.sin_port = htons(port_);
        } else {
            server.sin_addr.s_addr = ssp_addr_;
            server.sin_port = htons(ssp_port_);
        }

        logself(DEBUG, " --> connecting to server in_addr_t %u:%u",
                 server.sin_addr.s_addr, server.sin_port);

        myassert(!ev_);
        ev_ = new myevent_socket_t(
            evbase_, fd_, mev_readcb, NULL, mev_eventcb, this);
        myassert(ev_);
        ev_->set_logfn(logfn);

        rv = ev_->socket_connect((struct sockaddr*)(&server), sizeof(server));
        myassert (!rv || errno == EINPROGRESS);

        myassert(!write_to_server_enabled_);
        enable_write_to_server_();

        // struct timeval timeout = {10, 0}; // timeout connect attempt
        // myassert(0 == event_add(ev_, &timeout));

        logself(DEBUG, " transition to state CONNECTING");
        state_ = CONNECTING;
    }

done:
    logself(DEBUG, "fd_ = %d", fd_);
    logself(DEBUG, "done");
    return rv;
}

void
Connection::send_()
{
    int rv = 0;
    if (use_spdy_) {
        spdylay_session* session_ = this->spdysess_;
        if (spdylay_session_want_write(session_)) {
            if ((rv = spdylay_session_send(session_)) != 0) {
                disconnect();
                if (SPDYLAY_ERR_EOF == rv) {
                    logself(DEBUG, "remote peer closed");
                    on_eof();
                } else {
                    logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
                          "spdylay_session_send() returned \"%s\"",
                          spdylay_strerror(rv));
                    on_error();
                }
                return;
            }

            /* after writing, there's likely need to receive */
            ev_->set_readcb(mev_readcb);
        }
        else {
            // shadow's epoll doesn't seem to support edge-triggering,
            // so we do it ourselves: spdylay doesn't want to write,
            // so disable monitoring of write event
            disable_write_to_server_();
        }
        if(rv == 0) {
            if(spdylay_session_want_read(session_) == 0 &&
               spdylay_session_want_write(session_) == 0) {
                myassert(0);
                rv = -1;
            }
        }
    } else {
        http_write_to_outbuf();
        if (evbuffer_get_length(outbuf_) > 0) {
            // for some reason evbuffer_write() fails with "bad file
            // descriptor". so we have to read(fd_) ourselves. use iov
            // to avoid additional copying.

            // const int numwritten = evbuffer_write(outbuf_, fd_);

            struct evbuffer_iovec v[1];
            int numdrained = 0;
            const int n = evbuffer_peek(
                outbuf_, -1, NULL, v, ARRAY_LEN(v));
            for (int i = 0; i < n; ++i) {
                const int numwritten = write(
                    fd_, (const uint8_t *)v[i].iov_base, v[i].iov_len);
                if (numwritten == -1) {
                    myassert(errno == EWOULDBLOCK);
                    break;
                } else {
                    logself(DEBUG, "able to write %d bytes", numwritten);
                    cumulative_num_sent_bytes_ += numwritten;
                    numdrained += numwritten;
                    if (numwritten != v[i].iov_len) {
                        // couldn't write the whole iov -> move on
                        break;
                    }
                }
            }
            logself(DEBUG, "drained total of %d bytes", numdrained);
            myassert(0 == evbuffer_drain(outbuf_, numdrained));
            /* after writing, there's likely need to receive */
            ev_->set_readcb(mev_readcb);
        } else {
            /* since there's no data waiting to go out from the output
             * buffer, we disable monitoring of write event to reduce
             * unneeded triggering -- shadow doesn't support
             * edge-triggering yet it seems.
             */
            disable_write_to_server_();
        }
    }
    return;
}

void
Connection::on_socks5_proxy_readable()
{
    logself(DEBUG, "begin");
    if (socks5_state_ == SOCKS5_GREETING) {
        // this should be the first accept response from socks5 proxy
        logself(DEBUG, "process greeting response");
        char mem[2];
        myassert(2 == read(fd_, mem, 2));
        myassert(0 == memcmp(mem, "\x05\x00", 2));

        logself(DEBUG, "transition to SOCKS5_WRITE_REQUEST_NEXT");
        socks5_state_ = SOCKS5_WRITE_REQUEST_NEXT;
        ev_->set_writecb(mev_socks5_proxy_writecb);
        ev_->set_readcb(NULL);
    }
    else if (socks5_state_ == SOCKS5_READ_RESP_NEXT) {
        // this should be the succeeded response

        // even though we request to connect with a \x03 domain name,
        // the response will still be \x01 (this might or might not be
        // specific to Tor?)
        unsigned char mem[10]; /* read 10 bytes */
        myassert(10 == read(fd_, mem, 10));
#if 0
        printhex("socks proxy greeting response", mem, 4);
#endif
        if (0 != memcmp(mem, "\x05\x00\x00\x01", 4)) {
            /* we don't handle this yet, so just fail the cnx */
            on_error();
        } else {
            /* XXX/TODO: proxy might tell us to connect to another
             * ip/port. need to handle that
             */

            logself(DEBUG, "now tunnel is established through socks5 proxy");
            state_ = CONNECTED;
            socks5_state_ = SOCKS5_NONE;

            logself(DEBUG, "update the callbacks");
            ev_->setcb(mev_readcb, NULL, mev_eventcb, this);
            myassert(!write_to_server_enabled_);
            enable_write_to_server_();
        }
    }

    logself(DEBUG, "done");
    return;
}

void
Connection::on_socks5_proxy_writable()
{
    logself(DEBUG, "begin");
    if (socks5_state_ == SOCKS5_WRITE_REQUEST_NEXT) {
        const in_addr_t& addr = (ssp_addr_) ? ssp_addr_ : addr_;
        in_port_t port = (ssp_addr_) ? ssp_port_ : port_;
        myassert(addr != 0);
        myassert(port != 0);
            
        logself(DEBUG, "write the socks5 request to connect to %u:%u",
                addr, port);
        std::string req("\x05\x01\x00\x01", 4);
        req.append((const char*)&addr, 4);
        port = htons(port);
        req.append((const char*)&port, 2);
        myassert(write(fd_, req.c_str(), req.size()) == req.size());
#if 0
        printhex("i write request: ",
                 (const unsigned char*)req.c_str(), req.size());
#endif
        logself(DEBUG, "transition to SOCKS5_READ_RESP_NEXT");
        socks5_state_ = SOCKS5_READ_RESP_NEXT;
        //need to disable writing, otherwise it keeps reporting writable
        ev_->set_readcb(mev_socks5_proxy_readcb);
        ev_->set_writecb(NULL);
    }
    logself(DEBUG, "done");
}

ssize_t
Connection::spdylay_send_cb(spdylay_session *session, const uint8_t *data,
                            size_t length, int flags)
{
    logself(DEBUG, "begin, length=%d", length);
    myassert(session == spdysess_);
    Connection *conn = this;

    ssize_t retval = SPDYLAY_ERR_CALLBACK_FAILURE;
    ssize_t numsent = send(conn->fd_, data, length, 0);
    if (numsent < 0) {
        if (errno == EWOULDBLOCK) {
            retval = SPDYLAY_ERR_WOULDBLOCK;
            logself(DEBUG, " can't send but not error; only EWOULDBLOCK", numsent);
        } else {
            logWARN("error with errno %d", errno);
            retval = SPDYLAY_ERR_CALLBACK_FAILURE;
        }
    } else if (numsent == 0) {
        retval = SPDYLAY_ERR_CALLBACK_FAILURE;
        logWARN("sent %d bytes --> error", numsent);
    } else {
        retval = numsent;
        cumulative_num_sent_bytes_ += numsent;
        logself(DEBUG, "able to send %d bytes", numsent);
    }

    logself(DEBUG, "returning %d", retval);
    return retval;
}


ssize_t
Connection::spdylay_recv_cb(spdylay_session *session, uint8_t *buf,
                            size_t length, int flags)
{
    logself(DEBUG, "begin, length=%d", length);
    if (session != spdysess_) {
        /* XXX/TODO: temporary bandage. need to handle better */
        return 0;
    }
    Connection *conn = this;
    ssize_t retval = SPDYLAY_ERR_CALLBACK_FAILURE;
    ssize_t numread = recv(conn->fd_, buf, length, 0);

    if (0 == numread) {
        logself(DEBUG, "no more data is available for reading");
        retval = SPDYLAY_ERR_EOF;
    }
    else if (-1 == numread) {
        logself(DEBUG, "numread is -1");
        if(errno == EWOULDBLOCK) {
            logself(DEBUG, "it's ok, just EWOULDBLOCK");
            retval = SPDYLAY_ERR_WOULDBLOCK;
        } else {
            logWARN("error with errno %d", errno);
            retval = SPDYLAY_ERR_CALLBACK_FAILURE;
        }
    }
    else {
        logself(DEBUG, "able to read %zd bytes", numread);
        retval = numread;
        if (0 == cumulative_num_recv_bytes_
            && cnx_first_recv_byte_cb_)
        {
            cnx_first_recv_byte_cb_(this);
        }
        cumulative_num_recv_bytes_ += numread;
    }

    logself(DEBUG, "returning %d", retval);
    return retval;
}

void
Connection::spdylay_on_data_recv_cb(spdylay_session *session,
                                    uint8_t flags, int32_t stream_id,
                                    int32_t len)
{
    myassert(session == spdysess_);
    Connection *conn = this;
    const int32_t sid = stream_id;
    logself(DEBUG, "begin, sid %d, len %d, cnx %u",
            sid, len, conn->instNum_);
    if ((flags & SPDYLAY_DATA_FLAG_FIN) != 0) {
        logself(DEBUG, "last data frame of resource");

        if (inSet(psids_, sid)) {
            notify_pushed_body_done_(sid, this, cb_data_);
            return;
        }

        myassert(inMap(conn->sid2req_, sid));
        Request* req = conn->sid2req_[sid];
        req->notify_rsp_body_done();
    }
    logself(DEBUG, "done");
}

void
Connection::spdylay_on_data_chunk_recv_cb(spdylay_session *session,
                                          uint8_t flags, int32_t stream_id,
                                          const uint8_t *data, size_t len)
{
    myassert(session == spdysess_);
    Connection *conn = this;
    const int32_t sid = stream_id;
    logself(DEBUG, "begin, sid %d, len %d, cnx %u",
            sid, len, conn->instNum_);

    if (inSet(psids_, sid)) {
        notify_pushed_body_data_(sid, data, len, this, cb_data_);
        return;
    }

    myassert(inMap(conn->sid2req_, sid));
    Request* req = conn->sid2req_[sid];
    req->notify_rsp_body_data(data, len);

    logself(DEBUG, "done");
}

void
Connection::handle_server_push_ctrl_recv(spdylay_frame *frame)
{
    // must include Associated-To-Stream-ID
    const int32_t pushsid = frame->syn_stream.stream_id;
    const int32_t assoc_sid = frame->syn_stream.assoc_stream_id;

    logself(DEBUG, "begin, server-pushed stream %d with assoc id %d",
            pushsid, assoc_sid);

    myassert (assoc_sid != 0);

    char **nv = frame->syn_stream.nv;
    const char *content_length = 0;
    const char *pushedurl = NULL;
    unsigned int code = 0;
    //const char *content_length = 0;
    ssize_t contentlen = -1;

    for(size_t i = 0; nv[i]; i += 2) {
        if(strcmp(nv[i], ":status") == 0) {
            code = strtoul(nv[i+1], 0, 10);
            myassert(code == 200);
        } else if(nv[i][0] != ':') {
            if(strcasecmp(nv[i], "content-length") == 0) {
                contentlen = strtoul(nv[i+1], 0, 10);
            }
        } else if(strcmp(nv[i], ":pushedurl") == 0) {
            pushedurl = nv[i+1];
        }
    }

    logself(DEBUG, "pushed url: [%s], contentlen %d", pushedurl, contentlen);

    //sid2psids_[assoc_sid].push_back(pushsid);
    psids_.insert(pushsid);

    notify_pushed_meta_(
        pushsid, pushedurl, contentlen, (const char**)nv, this, cb_data_);

    logself(DEBUG, "done");
}
    
void
Connection::spdylay_on_ctrl_recv_cb(spdylay_session *session,
                                    spdylay_frame_type type,
                                    spdylay_frame *frame)
{
    logself(DEBUG, "begin");
    myassert(session == spdysess_);
    Connection *conn = this;
    switch(type) {
    case SPDYLAY_SYN_STREAM:
        handle_server_push_ctrl_recv(frame);
        break;
    case SPDYLAY_RST_STREAM:
        myassert(0);
        break;
    case SPDYLAY_SYN_REPLY: {
        const int32_t sid = frame->syn_reply.stream_id;
        myassert(inMap(conn->sid2req_, sid));
        Request* req = conn->sid2req_[sid];
        myassert(req);
        logself(DEBUG,
                "SYN REPLY sid: %d, cnx: %u, req: %u",
                sid, conn->instNum_, req->instNum_);

        char **nv = frame->syn_reply.nv;
        const char *status = 0;
        const char *version = 0;
        const char *content_length = 0;
        unsigned int code = 0;
        for(size_t i = 0; nv[i]; i += 2) {
            logself(DEBUG, "name-value pair: %s: %s", nv[i], nv[i+1]);
            if(strcmp(nv[i], ":status") == 0) {
                code = strtoul(nv[i+1], 0, 10);
                myassert(code == 200);
                status = nv[i+1];
            } else if(strcmp(nv[i], ":version") == 0) {
                version = nv[i+1];
            } else if(nv[i][0] != ':') {
                // if(strcmp(nv[i], "content-length") == 0) {
                //     content_length = nv[i+1];
                //     downstream->content_length_ = strtoul(content_length, 0, 10);
                // }
                //downstream->add_response_header(nv[i], nv[i+1]);
            }
        }
        if(!status || !version) {
            myassert(0);
            return;
        }
        logself(DEBUG, "notifying of response meta");
        req->notify_rsp_meta(code, nv);
    }
    default:
        break;
    }
    logself(DEBUG, "done");
}

void
Connection::spdylay_before_ctrl_send_cb(spdylay_session *session,
                                        spdylay_frame_type type,
                                        spdylay_frame *frame)
{
    logself(DEBUG, "begin");
    Connection *conn = this;
    myassert(session == spdysess_);
    if(type == SPDYLAY_SYN_STREAM) {
        const int32_t sid = frame->syn_stream.stream_id;
        Request *req = reinterpret_cast<Request*>(
            spdylay_session_get_stream_user_data(session, sid));
        myassert(req);
        logself(DEBUG, "new SYN sid: %d, cnx: %u, req: %u",
                sid, conn->instNum_, req->instNum_);
        conn->sid2req_[frame->syn_stream.stream_id] = req;
        req->dump_debug();
        req->notify_req_about_to_send();
    }
    logself(DEBUG, "done");
}
