#ifndef CONNECTION_HPP
#define CONNECTION_HPP

#include <unistd.h> /* close */
#include <string.h> /* memset */
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <netdb.h>
#include "myassert.h"
#include <time.h>
#include <shd-library.h>
#include <event2/event.h>
#include <event2/buffer.h>

#include <boost/function.hpp>

#include "myevent.hpp"
#include "common.hpp"
#include "request.hpp"

#include <spdylay/spdylay.h>

#include <string>
#include <map>
#include <set>
#include <queue>
#include <deque>

class Connection;

typedef boost::function<void(Connection*)> ConnectionErrorCb;
typedef boost::function<void(Connection*)> ConnectionEOFCb;
typedef boost::function<void(Connection*)> ConnectionFirstRecvByteCb;

typedef boost::function<void(Connection*, const Request*)> ConnectionRequestDoneCb;

typedef void (*PushedMetaCb)(int id, const char* url, ssize_t contentlen,
                             const char **nv, Connection* cnx, void* cb_data);
typedef void (*PushedBodyDataCb)(int id, const uint8_t *data, size_t len,
                                 Connection* cnx, void* cb_data);
typedef void (*PushedBodyDoneCb)(int id, Connection* cnx, void* cb_data);

/* this can be used for a connection towards a "server" (e.g.,
 * directly to webserver, or via a socks5 or spdy proxy).
 *
 * it can talk basic http or spdy with the "server".
 *
 * caveat: if http, chunked encoding not supported, i.e., response
 * must provide content length.
 *
 * submit requests onto this connection by calling
 * submit_request(). the request object will be notified of "meta"
 * (status and headers), body_data, and body_done via callbacks.
 */


class Connection
{
public:

    /*
     * !!!! the ports should be in host byte order, e.g., 80 for web.
     *
     *
     * if want to use socks5 proxy, both "socks5_addr" and
     * "socks5_port" must be non-zero.
     *
     * the constructor will immediately attempt to establish cnx to
     * the server/proxy.
     *
     * "addr/port": of the final web server, "socks5_addr/port": of
     * the socks5 proxy, "ssp_addr/port": of the ssp proxy.
     *
     * if the final server addr is specified, then ssp addr must not,
     * and vice versa. (where "specified" means "non-zero.")
     */
    Connection(myevent_base *evbase,
               const in_addr_t& addr, const in_port_t& port,
               const in_addr_t& socks5_addr, const in_port_t& socks5_port,
               const in_addr_t& ssp_addr, const in_port_t& ssp_port,
               ConnectionErrorCb error_cb, ConnectionEOFCb eof_cb,
               PushedMetaCb pushed_meta_cb, PushedBodyDataCb pushed_body_data_cb,
               PushedBodyDoneCb pushed_body_done_cb,
               void *cb_data /* for error_cb and eof_cb */,
               const bool& use_spdy
        );
    ~Connection();

    /* !!!! NOTE: this request will not be copied, so the caller must
     * not free this request until it's been completed (either
     * successfully or erroneously)
    */
    int submit_request(Request* req);
    /* return true in connected state and there is no request/IO being
     * active/queued.
     */
    void set_first_recv_byte_cb(ConnectionFirstRecvByteCb cb) {
        cnx_first_recv_byte_cb_ = cb;
    }
    bool is_idle() const;
    size_t get_queue_size() const
    {
        return submitted_req_queue_.size() + active_req_queue_.size();
    }
    const size_t& get_total_num_sent_bytes() const
    {
        return cumulative_num_sent_bytes_;
    }
    const size_t& get_total_num_recv_bytes() const
    {
        return cumulative_num_recv_bytes_;
    }

    std::queue<Request*> get_active_request_queue() const;
    std::deque<Request*> get_pending_request_queue() const;

    void set_request_done_cb(ConnectionRequestDoneCb cb);

    /* schedule this cnx for later deletion */
    void deleteLater(ShadowCreateCallbackFunc scheduleCallback);

    void on_read();
    void on_write();
    void on_connect();
    void on_error();
    void on_eof();

    void on_socks5_proxy_connected();
    void on_socks5_proxy_readable();
    void on_socks5_proxy_writable();

    ssize_t spdylay_send_cb(spdylay_session *session, const uint8_t *data,
                            size_t length, int flags);
    ssize_t spdylay_recv_cb(spdylay_session *session, uint8_t *buf,
                            size_t length, int flags);
    void spdylay_on_ctrl_recv_cb(spdylay_session *session,
                                 spdylay_frame_type type,
                                 spdylay_frame *frame);
    void spdylay_before_ctrl_send_cb(spdylay_session *session,
                                     spdylay_frame_type type,
                                     spdylay_frame *frame);

    /* a spdy DATA frame might be notified via multiple
     * spdylay_on_data_chunk_recv_cb() calls, then finally a
     * spdylay_on_data_recv_cb() call. to know when a response has
     * been completely received, should look at the flags inside the
     * spdylay_on_data_recv_cb()
     */
    void spdylay_on_data_chunk_recv_cb(spdylay_session *session,
                                       uint8_t flags, int32_t stream_id,
                                       const uint8_t *data, size_t len);
    void spdylay_on_data_recv_cb(spdylay_session *session,
                                 uint8_t flags, int32_t stream_id,
                                 int32_t len);

    const uint32_t instNum_; // monotonic id of this cnx obj
    const bool use_spdy_;

private:

    enum {
        // Disconnected
        DISCONNECTED,
        // Connecting proxy and making CONNECT request
        // PROXY_CONNECTING,
        // Tunnel is established with proxy
        // PROXY_CONNECTED,
        // Establishing tunnel is failed
        PROXY_FAILED,
        // Connecting to downstream and/or performing SSL/TLS handshake
        CONNECTING,
        // Connected to downstream
        CONNECTED,
        NO_LONGER_USABLE,
        // was connected and now destroyed, so don't use
        DESTROYED,
    };
    /* state of socks5 proxy */
    enum {
        SOCKS5_NONE,
        SOCKS5_GREETING,
        SOCKS5_WRITE_REQUEST_NEXT,
        SOCKS5_READ_RESP_NEXT,
    };

    int initiate_connection();

    /* this only takes data from whatever output buffers appropriate
     * (spdy or http) and write to socket. this is not responsible for
     * putting data into those buffers.
     */
    void send_(); /* need underscore, otherwise compile error because
                     same as socket send(2) */
    void disconnect();
    void http_write_to_outbuf(); // write submitted requests to output
                                 // buff
    // read from socket and process the read data
    bool http_receive();
    void handle_server_push_ctrl_recv(spdylay_frame *frame);

    static uint32_t nextInstNum;
    myevent_base *evbase_; // dont free
    // struct bufferevent *bev_;
    myevent_socket_t *ev_;
    int fd_;

    int state_;
    int socks5_state_;

    const in_addr_t addr_;
    const in_port_t port_;

    const in_addr_t socks5_addr_;
    const in_port_t socks5_port_;
    const in_addr_t ssp_addr_;
    const in_port_t ssp_port_;

    ConnectionErrorCb cnx_error_cb_;
    ConnectionEOFCb cnx_eof_cb_;
    ConnectionFirstRecvByteCb cnx_first_recv_byte_cb_;
    void *cb_data_;
    PushedMetaCb notify_pushed_meta_;
    PushedBodyDataCb notify_pushed_body_data_;
    PushedBodyDoneCb notify_pushed_body_done_;

    ConnectionRequestDoneCb notify_request_done_cb_;

    /* for spdy-to-server support */
    spdylay_session *spdysess_;
    /* dont free these Request's. these are only shallow pointer */
    std::map<int32_t, Request*> sid2req_;
    /* set of pushed stream ids */
    std::set<int32_t> psids_;
    /* for http-to-server support */

    // perhaps take a look at libevent's evhttp

    /* requests submitted by browser are enqueued in
     * submitted_req_queue_. when a request is written into the
     * outbuf_, it is moved to active_req_queue_.
     */
    std::deque<Request* > submitted_req_queue_; // dont free these ptrs
    std::queue<Request* > active_req_queue_; // dont free these ptrs
    struct evbuffer* inbuf_;
    struct evbuffer* outbuf_;
    const bool do_pipeline_; /* have NOT tested pipeling feature */
    uint32_t max_pipeline_size_; /* NOTE: currently we assume all
                                  * servers support size 4 */
    int http_rsp_state_;
    enum {
        HTTP_RSP_STATE_STATUS_LINE, /* waiting for a full status line */
        HTTP_RSP_STATE_HEADERS,
        HTTP_RSP_STATE_BODY,
    };
    int http_rsp_status_;
    std::vector<char *> rsp_hdrs_; // DO free every _other_ one of
                                   // these ptrs (i.e., index 0, 2, 4,
                                   // etc)
    size_t first_byte_pos_; // copied from the request obj. will
                            // include a range request header only if
                            // first_byte_pos_ > 0.
    ssize_t body_len_; // -1, or amount of data _left_ to read from
                       // server/deliver to user. this is of the
                       // response body only, and not of the full
                       // entity.

    /* total num bytes sent/received on this cnx (not counting the
     * socks handshake, which is negligible) */
    size_t cumulative_num_sent_bytes_;
    size_t cumulative_num_recv_bytes_;

    /* use a flag to avoid unnecessarily -- though not affecting
     * correctness -- calling the event's methods()
     */
    bool write_to_server_enabled_;
    void enable_write_to_server_();
    void disable_write_to_server_();

};

#endif /* CONNECTION_HPP */
