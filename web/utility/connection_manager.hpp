#ifndef CONNECTION_MANAGER_HPP
#define CONNECTION_MANAGER_HPP

#include <list>
#include <map>
#include <queue>
#include <utility>

#include <boost/function.hpp>

#include <shd-library.h>

#include "myevent.hpp"
#include "request.hpp"
#include "connection.hpp"


class ConnectionManager
{
public:

    typedef boost::function<void(Request*)> RequestErrorCb;

    /* "request_error_cb": will be called with the failed Request.
     *
     * Do NOT destroy the ConnectionManager object within the
     * "request_error_cb" stack.
     */
    ConnectionManager(myevent_base *evbase, 
                      const in_addr_t& socks5_addr, const in_port_t& socks5_port,
                      RequestErrorCb request_error_cb,
                      const uint8_t max_persist_cnx_per_srv=8,
                      const uint8_t max_retries_per_resource=2);
    ~ConnectionManager();

    void submit_request(Request *req);
    void reset();

    uint64_t get_timestamp_recv_first_byte() const { return timestamp_recv_first_byte_; }
    void get_total_bytes(size_t& tx, size_t& rx);

    /* schedule this for later deletion */
    void deleteLater(ShadowCreateCallbackFunc scheduleCallback);

    const uint32_t instNum_; // monotonic id of this conn manager obj

    typedef std::pair<std::string, uint16_t> NetLoc;

private:
    static uint32_t nextInstNum;

    ConnectionManager(ConnectionManager const&);
    void operator=(ConnectionManager const&);

    /* to receive notification from Connection object. */
    void cnx_first_recv_byte_cb(Connection*);
    void cnx_error_cb(Connection*, const NetLoc&);
    void cnx_eof_cb(Connection*, const NetLoc&);
    void cnx_request_done_cb(Connection*, const Request*, const NetLoc&);

    bool retry_requests(std::queue<Request*> requests);
    void handle_unusable_conn(Connection*, const NetLoc&);
    void release_conn(Connection*, const NetLoc&);

    struct Server
    {
    public:
        ~Server();

        std::list<Request*> requests_;
        std::list<Connection*> connections_;
    };

    myevent_base *evbase_; // dont free
    const in_addr_t socks5_addr_;
    const in_port_t socks5_port_;
    uint8_t max_persist_cnx_per_srv_;
    uint8_t max_retries_per_resource_;

    uint64_t timestamp_recv_first_byte_;
    size_t totaltxbytes_;
    size_t totalrxbytes_;

    RequestErrorCb notify_req_error_;

    /* map key is "[hostname, port]" pair.
     */
    std::map<NetLoc, Server*> servers_;
};

#endif /* CONNECTION_MANAGER_HPP */
