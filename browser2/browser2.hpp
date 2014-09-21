#ifndef BROWSER_HPP
#define BROWSER_HPP

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
#include <time.h>
#include <shd-library.h>
#include <boost/random.hpp>
#include <boost/generator_iterator.hpp>

#include <openssl/evp.h>

#include "myevent.hpp"

#include <map>
#include <list>
#include <string>
#include <queue>

#include "request.hpp"
#include "connection.hpp"
#include "common.hpp"
#include "shd-html.hpp"


/* make shd-cdf happy. we don't want the memory checks. */
#define MAGIC_VALUE
#define MAGIC_DECLARE
#define MAGIC_INIT(object)
#define MAGIC_ASSERT(object)
#define MAGIC_CLEAR(object)

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include "shd-cdf.h"

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif

#define MD5_HEX_DIGEST_LEN (32)

enum browser_state {
    SB_INIT = 0,
    SB_FETCHING_DOCUMENT,
    SB_DONE_DOCUMENT,
    SB_FETCHING_EMBEDDED,
    SB_DONE,
    SB_CLOSED, /* dont do anything more */
};

class browser_t
{
public:
    browser_t();
    ~browser_t();

    void start(int argc, char *argv[]);
    void request_about_to_send_cb(Request* req);
    void response_meta_cb(const int& status, char **headers, Request* req);
    void response_body_data_cb(const uint8_t *data, const size_t& len, Request* req);
    void response_body_done_cb(Request* req);
    void connection_error_cb(Connection* conn);
    void connection_eof_cb(Connection* conn);
    void connection_first_recv_byte_cb(Connection* conn);
    void activate(const bool blocking);
    void on_notified();
    void on_timeout_timer_fired(const uint32_t loadnum);
    void on_delayed_load_timer_fired(const std::string& url);

    /* close any current/future download, to be ready for freeing */
    void close();

    const uint32_t instNum_; // monotonic id of this browser obj
    static const EVP_MD* digest_algo_; /* which digest to use. dont free. */

    /* need a load timeout context because we can't cancel a scheduled
     * callback, so when it does get call, the callback needs to make
     * sure it's not cancelling an incorrect page load in progress */
    class LoadTimeoutCtx_t
    {
    public:
        LoadTimeoutCtx_t(browser_t* browser, const uint32_t& loadnum)
            : b_(browser), loadnum_(loadnum) {}
        browser_t *b_;
        const uint32_t loadnum_;
    };

    class DelayedLoadCtx_t
    {
    public:
        DelayedLoadCtx_t(browser_t* browser, const std::string& url)
            : b_(browser), url_(url) {}
        browser_t *b_;
        const std::string url_;
    };

private:

    typedef struct _ExpectedObj {
        size_t bodySize;
        /* if hexMd5Digest == NULL, then don't check the digest */
        std::string* hexMd5Digest;
    } ExpectedObj;

    // use this for every load
    std::map<std::string, ExpectedObj> expected_objects_;

    class PageSpec {
    public:
        PageSpec(const std::string& url) : url_(url) {}
        ~PageSpec();

        const std::map<std::string, ExpectedObj>& get_expected_objects() const
        {
            return expected_objects_;
        }

        void
        add_expected_object(const std::string& url,
                            const ExpectedObj& eo);

        const std::string url_;

    private:
        std::map<std::string, ExpectedObj> expected_objects_;
    };

    std::vector<PageSpec*> page_specs_;

    void validate_one_resource(const std::string& url,
                               const size_t& actual_body_size,
                               const char* actual_digest,
                               std::map<std::string, ExpectedObj>& expected_objects);
    void verify_page_load();
    void report_result() const;
    void report_failed_load(const char *reason) const;
    /* reset state so that we're ready to load another page */
    void reset();
    void close_all_connections();
    void request_embedded_objects();
    void handle_unusable_conn(Connection *conn);
    bool retry_requests(std::queue<Request*> requests);
    /* schedule the cnx for deletion (in whichever state it is) and
     * then forget about it */
    void release_conn(Connection *conn);

    static uint32_t nextInstNum;
    
    enum browser_state state;

    myevent_base* evbase_;

    std::string first_hostname_;
    /* We never change them during simumlation */
    std::string socks5_host_;
    in_addr_t socks5_addr_;
    uint16_t socks5_port_;
    /* map key is netloc ("hostname:port") */
    std::map<std::string, std::list<Connection*> > connections_;
    int max_persist_cnx_per_srv_;

    /* statistics */
    size_t totalbodybytes_; /* only response bodies */
    size_t totaltxbytes_; /* all tx bytes for _one_ page load */
    size_t totalrxbytes_; /* all rx bytes for _one_ page load */
    uint16_t totalnumobjects_;
    uint16_t totalnumerrorobjects_;
    uint64_t load_start_timepoint_;
    uint64_t load_done_timepoint_;

    bool doc_is_html_;
    intptr_t doc_req_instNum_; /* request for the main document */
    uint32_t doc_expected_len_;

    // not yet complete requests. once a request is complete, should
    // remove it from here. the set element is the url, e.g.,
    // "http://www.foo.com/index.html", i.e., dont specify the port
    // unless it's part of the url
    std::map<std::string, Request*> pending_requests_;
    /* urls of requests that we have fully received */
    std::set<std::string> received_resources_;

    /* urls of all known embedded resources, that will be fetched. the
     * page load is considered complete when received_resources_ set
     * equals embedded_resources_ set  */
    std::set<std::string> embedded_resources_;

    /* if the main doc is html, then save its content here. */
    std::string doc_content;
    /* map key is Request's instNum_ */
    std::map<uintptr_t, EVP_MD_CTX*> req2mdctx;
    /* map key is Request's instNum_, value is the text of the
     * script */
    std::map<uintptr_t, std::string> scriptReq2BodyText;
    uint64_t time_tsfb_; // time sending first byte
    uint64_t time_trfb_; // time receiving first byte
    bool do_spdy_;
    CumulativeDistribution* think_times_cdf;
    boost::variate_generator<boost::mt19937, boost::uniform_real<> > *think_time_rand_gen;

    Connection* get_connection(const char* hostname, const uint16_t port);
    void request_one_url(const char* url);
    void process_a_script(const ScriptResource& sr);

    bool is_page_done() const;

    /* save this for repeated loading */
    //std::string url_;
    uint16_t page_specs_idx_; // which page we're loading
    /* monotonic id of the page load. also used to know whether to
     * cancel a page load. should increment this loadnum_ soon after
     * the load is finished, so that while sleeping waiting for the
     * _next_ page load, if the timeout timer expired for the one just
     * finished, it will mistakenly think the load timed out.
     */
    uint32_t loadnum_;

    void stop_load(); // stop current page load
    int32_t timeout_ms_;
    std::string myhostname_;

    /* i want an extra value of "init" for validate result intead of
     * just true/false to prevent accidentally believing validation
     * succeeds when it never took place: it should be init to
     * VR_NONE. when starting to load page, set it to VR_SUCCESS
     * (note: only once when starting to load the page). any failure
     * will set it to VR_FAIL.
     */
    typedef enum {
        VR_NONE,
        VR_FAIL,
        VR_SUCCESS,
    } validate_result_t;

    validate_result_t validate_result_;

    /* for "deferred" action, because sometimes we can't/don't want to
     * do things in a callback stack.
     *
     * notifier should use shadow's "createcallback" to schedule only
     * if notified_ is false. if it is true, then we are already
     * notified but haven't gotten around to handle it, so don't
     * schedule again.
     */
    bool notified_;
    void notify(const uint32_t delay_ms = 0);

    void load(const std::string& url);
    //////
};

void browser_start(browser_t* b, gint argc, gchar** argv);
void browser_free(browser_t* b);
void browser_activate(browser_t* b);

#endif /* BROWSER_HPP */
