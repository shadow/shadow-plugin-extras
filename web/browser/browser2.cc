#include "browser2.hpp"
#include "common.hpp"
#include "shd-html.hpp"
#include "myassert.h"

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <fstream>
#include <sstream>

#include <boost/lexical_cast.hpp>
#include <errno.h>
#include <math.h>
#include <sys/time.h>
#include <unistd.h>
#include <fcntl.h>              /* Obtain O_* constant definitions */

#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include <vector>

using std::vector;
using std::map;
using std::set;
using std::string;
using std::list;
using std::queue;

#ifdef __cplusplus /* If this is a C++ compiler, use C linkage */
extern "C" {
#endif

#include "shd-url.h"

#ifdef __cplusplus /* If this is a C++ compiler, end C linkage */
}
#endif


using boost::lexical_cast;

extern ShadowLogFunc logfn;
extern ShadowCreateCallbackFunc scheduleCallback;

#ifdef ENABLE_MY_LOG_MACROS
/* "inst" stands for instance, as in, instance of a class */
#define loginst(level, inst, fmt, ...)                                  \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, brwsr= %d): " fmt,  \
              __LINE__, (inst)->instNum_, ##__VA_ARGS__);               \
    } while (0)

/* like loginst, but shortcut "this" as instance */
#define logself(level, fmt, ...)                                        \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, brwsr= %d): " fmt,  \
              __LINE__, (this)->instNum_, ##__VA_ARGS__);               \
    } while (0)
#else
/* no-ops */

#define loginst(level, inst, fmt, ...)

#define logself(level, fmt, ...)

#endif


#define SPDY_PORT (81)

static const uint8_t g_max_retries_per_resource = 2;

uint32_t browser_t::nextInstNum = 0;
const EVP_MD* browser_t::digest_algo_ = NULL;

/* so we can avoid using the ptr in delayed callbacks */
static bool g_destroyed = false;

namespace {

static void
notified(gpointer ptr)
{
    browser_t *b = (browser_t*)ptr;
    if (g_destroyed) {
        return;
    }
    b->on_notified();
}

static void
timeout_timer_fired(gpointer ptr)
{
    browser_t::LoadTimeoutCtx_t* ltc = (browser_t::LoadTimeoutCtx_t*)ptr;
    if (g_destroyed) {
        return;
    }
    ltc->b_->on_timeout_timer_fired(ltc->loadnum_);
    delete ltc;
}
} // namespace

static void
printUsageAndExit(const char* prog)
{
    logCRITICAL(
"USAGE: %s --socks5 <host:port>|none --max-persist-cnx-per-srv ...|none\n"\
"          --page-spec <path>|none --think-times <path>|none\n"\
"          --timeoutSecs <path>|none --mode-spec <path>|none\n"\
"\n"\
"  * --mode-spec is a file that specifies each client's mode, vanilla or spdy.\n"\
"  * page spec contains specification of multiple pages to load: each page\n"\
"    spec begins with a line \"page-url: <url>\", and the following lines should\n"\
"    be \"objectURL | objectSize | objectMd5Sum (optional)\"\n"\
"  * if --think-times is none, then no think times between downloads; if it's\n"\
"    a number N > 1, then it's considered the upperbound of a uniform range\n"\
"    [1, N] millieconds; otherwise, it's assumed to be a path to a cdf file.\n"\
"", prog);
    exit(-1);
}

static void
parseValidateLine(const string& line,
                  string& url, size_t& bodySize, string& digeststr)
{
    std::istringstream iss(line);
    string sizestr;
    std::getline(iss, url, '|');
    boost::algorithm::trim(url);
    logDEBUG("url: [%s]", url.c_str());
    std::getline(iss, sizestr, '|');
    bodySize = strtol(sizestr.c_str(), NULL, 10);
    logDEBUG( "bodySize: [%d]", bodySize);
    myassert(bodySize > 0);
    std::getline(iss, digeststr, '|');
    boost::algorithm::trim(digeststr);
    logDEBUG("digest: [%s]", digeststr.c_str());
    return;
}

void
browser_t::start(int argc, char *argv[])
{
    const char *socks5_host = NULL;
    uint16_t socks5_port = 0;

    char *tmp = NULL; // dont free

    //XXX/ getopt() doesn't seem to work in shadow.

    myassert(argc == 13);

    char *socks5_host_port = argv[2];
    if (strcmp(socks5_host_port, "none")) {
        char* tmp = strchr(socks5_host_port, ':');
        myassert(tmp);
        *tmp = 0;
        ++tmp;
        socks5_host = socks5_host_port;
        socks5_port = strtol(tmp, NULL, 10);
    }

    char *max_persist_cnx_per_srv_str = argv[4];
    if (strcmp(max_persist_cnx_per_srv_str, "none")) {
        max_persist_cnx_per_srv_ = lexical_cast<int>(max_persist_cnx_per_srv_str);
        myassert(max_persist_cnx_per_srv_ <= 12);
        myassert(max_persist_cnx_per_srv_ > 0);
    }
    logfn(SHADOW_LOG_LEVEL_INFO, __func__,
          "Max persistent connections per server: %d", 
          max_persist_cnx_per_srv_);
    
    const char *pagespecfile = argv[6];
    if (strcmp(pagespecfile, "none")) {
        logself(DEBUG, "loading pagespecfile %s", pagespecfile);
        std::ifstream infile(pagespecfile, std::ifstream::in);
        if (!infile.good()) {
            logfn(SHADOW_LOG_LEVEL_CRITICAL, __func__,
                  "error: can't read page-spec file %s", pagespecfile);
            myassert(0);
        }
        string line;
        while (std::getline(infile, line)) {
            logself(DEBUG, "line: [%s]", line.c_str());
            if (line.length() == 0 || line.at(0) == '#') {
                /* empty lines and lines beginining with '#' are
                   ignored */
                continue;
            } else if (boost::starts_with(line.c_str(), "page-url: ")) {
                std::istringstream iss(line);
                string token;
                std::getline(iss, token, ' '); // get rid of page-url:
                std::getline(iss, token, ' '); // now get the url
                boost::algorithm::trim(token);
                myassert(token.length() > 0);
                logself(DEBUG, "page spec url: [%s]", token.c_str());
                page_specs_.push_back(new PageSpec(token));
            } else {
                string _url, digeststr;
                ExpectedObj eo = {0, NULL};
                parseValidateLine(line, _url, eo.bodySize, digeststr);
                myassert(_url.length() > 0);
                myassert(eo.bodySize > 0);
                if (digeststr.length() > 0) {
                    /* digest is optional */
                    myassert(digeststr.length() == MD5_HEX_DIGEST_LEN);
                    eo.hexMd5Digest = new string(digeststr);
                    myassert(eo.hexMd5Digest);
                } else {
                    eo.hexMd5Digest = NULL;
                }
                page_specs_.back()->add_expected_object(_url, eo);
                logself(DEBUG, "num eo %d",
                        page_specs_.back()->get_expected_objects().size());
            }
        }

        logself(DEBUG, "number of page specs %d", page_specs_.size());
        myassert(page_specs_.size() > 0);
    }

    const char *thinktimes_arg = argv[8];
    if (!strcmp(thinktimes_arg, "none")) {
        // --> no wait time between downloads
    } else {
        try {
            // is it a number? then that's the upper bound of a uniform
            // think time range [1, number] in milliseconds.
            static const int lowerbound_thinktime_ms = 1;
            int upperbound = boost::lexical_cast<int>(thinktimes_arg);
            logself(DEBUG, "thinktimes_arg is a number %d", upperbound);
            myassert(upperbound > lowerbound_thinktime_ms);
            think_time_rand_gen =
                new boost::variate_generator<boost::mt19937, boost::uniform_real<double> >(
                    boost::mt19937(std::time(0)), boost::uniform_real<double>(
                        lowerbound_thinktime_ms, upperbound));
            logself(DEBUG, "--> picking uniform thinktimes in range [%d, %d]",
                    lowerbound_thinktime_ms, upperbound);
        }
        catch(boost::bad_lexical_cast& e) {
            logself(DEBUG,
                    "thinktimes_arg not a number --> assume it's a cdf file");
            think_times_cdf = cdf_new(0, thinktimes_arg);
            myassert(think_times_cdf);

            think_time_rand_gen =
                new boost::variate_generator<boost::mt19937, boost::uniform_real<double> >(
                    boost::mt19937(std::time(0)), boost::uniform_real<double>(0.0, 1.0));
        }
    }

    char *timeoutSecs_str = argv[10];
    if (strcmp(timeoutSecs_str, "none")) {
        const uint32_t sec = lexical_cast<uint32_t>(timeoutSecs_str);
        myassert(sec >= 1);
        myassert(sec <= (10*60)); // 10 minutes
        timeout_ms_ = sec * 1000;
        logfn(SHADOW_LOG_LEVEL_INFO, __func__, "Timeout (s): %d", sec);
    } else {
        logfn(SHADOW_LOG_LEVEL_INFO, __func__, "Timeout (s): none");
    }

    const char *modespecfile = argv[12];
    if (strcmp(modespecfile, "none")) {
        logself(DEBUG, "loading modespecfile %s", modespecfile);
        std::ifstream infile(modespecfile, std::ifstream::in);
        if (!infile.good()) {
            logfn(SHADOW_LOG_LEVEL_CRITICAL, __func__,
                  "error: can't read mode-spec file %s", modespecfile);
            myassert(0);
        }
        string line;
        bool found = false;
        while (std::getline(infile, line)) {
            logself(DEBUG, "line: [%s]", line.c_str());
            if (line.length() == 0 || line.at(0) == '#') {
                /* empty lines and lines beginining with '#' are
                   ignored */
                continue;
            } else if (boost::starts_with(line.c_str(), myhostname_.c_str())) {
                std::istringstream iss(line);
                string token;
                std::getline(iss, token, '='); // get rid of mode-url:
                std::getline(iss, token, '='); // now get the mode
                boost::algorithm::trim(token);
                myassert(token.length() > 0);
                logself(DEBUG, "mode: [%s]", token.c_str());
                if (token == "vanilla") {
                    do_spdy_ = false;
                } else if (token == "spdy") {
                    do_spdy_ = true;
                } else {
                    logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                          "invalid mode \"%s\"in mode-spec file %s",
                          token.c_str(), modespecfile);
                    myassert(false);
                }
                found = true;
                break;
            }
        }
        if (!found) {
            logfn(SHADOW_LOG_LEVEL_ERROR, __func__,
                  "cannot find my mode in mode-spec file %s", modespecfile);
            myassert(0);
        }
    }


    if (socks5_host) {
        socks5_host_ = socks5_host;
        socks5_addr_ = getaddr(socks5_host_.c_str());
    }
    socks5_port_ = socks5_port;

    logself(DEBUG, "socks5: %s:%d (in_addr_t = %u)",
            socks5_host_.c_str(), socks5_port_, socks5_addr_);

    ++loadnum_;

    // pick a random page to load
    page_specs_idx_ = rand() % page_specs_.size();
    expected_objects_ = page_specs_[page_specs_idx_]->get_expected_objects();
    logself(DEBUG, "loading idx [%d], expected_objects_ %d",
            page_specs_idx_, expected_objects_.size());
    load(page_specs_[page_specs_idx_]->url_);
}

void
browser_t::load(const string& url)
{
    // it seems the log statement will be reported by valgrind as
    // "possibly lost"
    logself(DEBUG, "begin, url is [%s]", url.c_str());

    gchar* hostname = NULL; // free
    uint16_t port = 80;
    gchar* path = NULL; // free

    myassert(0 == url_get_parts(url.c_str(), &hostname, &port, &path));
    myassert(hostname);
    myassert(path);

    logself(DEBUG, "parsed hostname [%s], and port %u", hostname, port);

    if (!connman_) {
        connman_ = new ConnectionManager(
            evbase_,
            socks5_addr_, socks5_port_,
            boost::bind(&browser_t::response_finished_cb, this, _1, false),
            max_persist_cnx_per_srv_,
            g_max_retries_per_resource);
        myassert(connman_);
    }

    if (timeout_ms_ > -1) {
        scheduleCallback(
            &timeout_timer_fired, new LoadTimeoutCtx_t(this, loadnum_),
            timeout_ms_);
    }

    Request* req = new Request(
        path, string(hostname), port, url, NULL,
        boost::bind(&browser_t::response_meta_cb, this, _1, _2, _3),
        boost::bind(&browser_t::response_body_data_cb, this, _1, _2, _3),
        boost::bind(&browser_t::response_finished_cb, this, _1, true)
        );

    string loadid = myhostname_;
    loadid += "-load-";
    loadid += lexical_cast<string>(loadnum_);
    req->add_header("x-load-id", loadid.c_str());
    connman_->submit_request(req);
    doc_req_instNum_ = req->instNum_;
    state = SB_FETCHING_DOCUMENT;
    validate_result_ = VR_SUCCESS;
    struct timeval t;
    myassert(0 == gettimeofday(&t, NULL));
    load_start_timepoint_ = gettimeofdayMs(&t);
    logself(DEBUG, "load_start_timepoint_ %d", load_start_timepoint_);
    pending_requests_[req->url_] = req;

    first_hostname_ = string(hostname);

    g_free(hostname);
    g_free(path);

    logself(DEBUG, "done");
    return;
}

void browser_free(browser_t* b) {
    /* Clean up */
    delete b;
}

void
browser_t::activate(const bool blocking)
{
    myassert(SB_CLOSED != state);
    if (blocking) {
        evbase_->dispatch();
    } else {
        evbase_->loop_nonblock();
    }
}

void
browser_t::response_meta_cb(const int& status, char **headers, Request* req)
{
    logself(DEBUG, "begin, req url [%s]", req->url_.c_str());
    myassert(status == 200 || status == 206);

    if (req->get_num_retries() == 0) {
        myassert(!inMap(req2mdctx, req->instNum_));
        if (inMap(expected_objects_, req->url_)
            && expected_objects_[req->url_].hexMd5Digest)
        {
            // set up for computin digest, only if makes sense/needed
            EVP_MD_CTX *mdctx = EVP_MD_CTX_create();
            EVP_DigestInit_ex(mdctx, digest_algo_, NULL);
            req2mdctx[req->instNum_] = mdctx;
        }

        ++totalnumobjects_;
        logself(DEBUG, "new totalnumobjects %d", totalnumobjects_);
    }

    if (req->instNum_ == doc_req_instNum_) {
        size_t i = 0;
        while (headers[i]) {
            myassert(headers[i+1]);
            const char *keystr = headers[i];
            logself(DEBUG, "hdr n= [%s] v= [%s]", keystr, headers[i+1]);
            if (!strcasecmp(keystr, "content-type")) {
                doc_is_html_ = (0 == strcasecmp(headers[i+1], "text/html"));
            }
            i += 2;
        }
    }

    logself(DEBUG, "done");
}

void
browser_t::response_body_data_cb(
    const uint8_t *data, const size_t& len, Request* req)
{
    logself(DEBUG, "begin, len %u", len);
    if (len > 0) {
        if (inMap(req2mdctx, req->instNum_)) {
            EVP_MD_CTX *mdctx = req2mdctx[req->instNum_];
            myassert(mdctx);
            EVP_DigestUpdate(mdctx, data, len);
        }

        if (req->instNum_ == doc_req_instNum_ && doc_is_html_) {
            logself(DEBUG, "main doc -> save data");
            doc_content.append((const char*)data, len);
        }
        else if (inMap(scriptReq2BodyText, req->instNum_)) {
            logself(DEBUG, "more data for script resource [%s]",
                    req->url_.c_str());
            scriptReq2BodyText[req->instNum_].append((const char*)data, len);
        }
    }
    totalbodybytes_ += len;
    logself(DEBUG, "new totalbodybytes_ %d", totalbodybytes_);
    logself(DEBUG, "done");
}

bool
browser_t::is_page_done() const
{
    logself(DEBUG, "begin");
    logself(DEBUG, "num embedded_resources_ [%u], num completed [%u]",
            embedded_resources_.size(), received_resources_.size());
    const bool done = embedded_resources_.size() == received_resources_.size();

    logself(DEBUG, "done, returning %u", done);
    return done;
}

void
browser_t::response_finished_cb(Request* req, bool success)
{
    logself(DEBUG, "begin");

    if (inMap(req2mdctx, req->instNum_)) {
        EVP_MD_CTX *mdctx = req2mdctx[req->instNum_];
        myassert(mdctx);
        unsigned char md_value[EVP_MAX_MD_SIZE];
        unsigned int md_len = 0;
        EVP_DigestFinal_ex(mdctx, md_value, &md_len);
        EVP_MD_CTX_destroy(mdctx);
        req2mdctx.erase(req->instNum_);
        char hex_digest[EVP_MAX_MD_SIZE * 2] = {0};
        to_hex(md_value, md_len, hex_digest);

        validate_one_resource(
            req->url_, req->get_body_size(), hex_digest, expected_objects_);
    } else {
        validate_one_resource(
            req->url_, req->get_body_size(), NULL, expected_objects_);
    }

    pending_requests_.erase(req->url_);
    logself(DEBUG, "done fetching url [%s]", req->url_.c_str());
    if (req->instNum_ == doc_req_instNum_) {
        logself(DEBUG, "done fetching main doc --> transition state");
        state = SB_DONE_DOCUMENT;
        notify();
    }
    else {
        myassert(state == SB_FETCHING_EMBEDDED);

        received_resources_.insert(req->url_);

        if (inMap(scriptReq2BodyText, req->instNum_)) {
            // process it
            ScriptResource sr;
            // leave the sr.src empty
            string& s = scriptReq2BodyText[req->instNum_];
            boost::trim(s);
            boost::split(sr.lines, s, boost::is_any_of("\n"));
            scriptReq2BodyText.erase(req->instNum_);
            process_a_script(sr);
        }

        if (is_page_done()) {
            /* we only compare the number of the resources. this might
             * miss cases where the counts equal but the two sets are
             * not equal, but that is considered a failed load anyway
             */

            logself(DEBUG,
                    "this is last embedbed resource -> transition to done");
            state = SB_DONE;
            notify();
        }
    }

    req->deleteLater(scheduleCallback);
    logself(DEBUG, "done");
}

static void
delayed_load_timer_fired(gpointer ptr)
{
    browser_t::DelayedLoadCtx_t* ctx = (browser_t::DelayedLoadCtx_t*)ptr;
    if (g_destroyed) {
        return;
    }
    ctx->b_->on_delayed_load_timer_fired(ctx->url_);
    delete ctx;
}

void
browser_t::process_a_script(const ScriptResource& sr)
{
    logself(DEBUG, "begin");
    if (sr.src.length()) {
        /* if there's a "src" field specified */
        myassert(0 == sr.lines.size());
        request_one_url(sr.src.c_str());
    } else {
        /* go through the script to schedule loads of resources loaded
         * by the script */
        vector<string>::const_iterator it = sr.lines.begin();
        for (; it != sr.lines.end(); ++it) {
            string line = *it;
            boost::trim(line);
            logself(DEBUG, "line [%s]", line.c_str());
            if (line.find("// delayed_load: ") != line.npos) {
                /* assume line has this format:
                 *
                 * "// delayed_load: url= <url> delayms= <delay>"
                 */
                vector<string> parts;
                boost::split(parts, line, boost::is_any_of(" "));
                myassert(parts[2] == "url=");
                const string& url_to_fetch = parts[3];
                myassert(0 < url_to_fetch.length());

#if 0
                scheduleCallback(&delayed_load_timer_fired,
                                 new DelayedLoadCtx_t(this, url_to_fetch),
                                 boost::lexical_cast<uint32_t>(parts[5]));

                /* we are scheduling the delayed load as way to
                 * simulate the script's execution time. but during
                 * the wait, the browser might finish loading
                 * everything else and have no other ones "in
                 * progress". so we have to somehow prevent it from
                 * thinking it's done.
                 */
#else
                /* for now, dont support delayed load. only do
                 * immediate loads */
                logself(DEBUG, "requesting a js-loaded resource [%s]",
                        url_to_fetch.c_str());
                request_one_url(url_to_fetch.c_str());
#endif
            }
        }
    }
    logself(DEBUG, "done");
}

void
browser_t::request_embedded_objects()
{
    vector<string> images;
    vector<ScriptResource> scripts;

    const gchar* html = doc_content.c_str();

    logself(DEBUG, "begin");

    html_parse(html, images, &scripts);

    logself(DEBUG, "done parsing html");
    doc_content.clear();

    logself(DEBUG, "num images: [%u]", images.size());
    vector<string>::const_iterator it = images.begin();

    for (; it != images.end(); ++it) {
        const char* url = it->c_str();
        request_one_url(url);
    }

    logself(DEBUG, "num scripts: [%u]", scripts.size());
    vector<ScriptResource>::const_iterator srit = scripts.begin();
    for (; srit != scripts.end(); ++srit) {
        process_a_script(*srit);
    }

    logself(DEBUG, "done");
}

void
browser_t::request_one_url(const char* url)
{
    gchar* hostname = NULL;
    gchar* path = NULL;
    uint16_t port = 80;

    logself(DEBUG, "got resource, url [%s]", url);

    embedded_resources_.insert(string(url));

    if (url_is_absolute(url)) {
        url_get_parts(url, &hostname, &port, &path);
    } else {
        hostname = g_strdup(first_hostname_.c_str());
            
        if (!g_str_has_prefix(url, "/")) {
            path = g_strconcat("/", url, (char*)NULL);
        } else {
            path = g_strdup(url);
        }
    }

    /// XXX what if the embedded resource has been already/being
    /// requested? e.g., multiple <img> tags pointing to the same
    /// url. for now, we don't allow that.
    myassert(!inMap(pending_requests_, string(url)));
    Request* req = new Request(
        path, string(hostname), port, string(url), NULL,
        boost::bind(&browser_t::response_meta_cb, this, _1, _2, _3),
        boost::bind(&browser_t::response_body_data_cb, this, _1, _2, _3),
        boost::bind(&browser_t::response_finished_cb, this, _1, true)
        );
    connman_->submit_request(req);
    pending_requests_[req->url_] = req;

    const size_t len = req->url_.length();
    if (len > 3 && req->url_.find(".js", len-3) != req->url_.npos) {
        /* its a javascript --> need to save its body text */
        scriptReq2BodyText[req->instNum_] = "";
    }
    
    g_free(path);
    g_free(hostname);

}

void
browser_t::on_delayed_load_timer_fired(const string& url)
{
    logself(DEBUG, "begin");

    request_one_url(url.c_str());

    logself(DEBUG, "done");
}

browser_t::~browser_t()
{
    g_destroyed = true;
    reset();
    vector<PageSpec*>::iterator it = page_specs_.begin();
    for (; it != page_specs_.end(); ++it) {
        delete (*it);
    }
    page_specs_.clear();
    if (evbase_) {
        delete evbase_;
        evbase_ = NULL;
    }
    if (think_times_cdf) {
        cdf_free(think_times_cdf);
        think_times_cdf = NULL;
    }
    if (think_time_rand_gen) {
        delete think_time_rand_gen;
        think_time_rand_gen = NULL;
    }
}

browser_t::browser_t()
    : instNum_(nextInstNum), state(SB_INIT), notified_(false)
    , think_time_rand_gen(NULL)
{
    ++nextInstNum;

    evbase_ = new myevent_base(logfn);
    myassert(evbase_);

    socks5_addr_ = 0;
    socks5_port_ = 0;
    do_spdy_ = false;
    max_persist_cnx_per_srv_ = 6; // default
    think_times_cdf = NULL;

    page_specs_idx_ = 0;
    loadnum_ = 0;
    timeout_ms_ = -1;
    connman_ = NULL;

    reset();

    char myhostname[80] = {0};
    myassert(0 == gethostname(myhostname, (sizeof myhostname) - 1));
    myhostname_ = myhostname;

    g_destroyed = false;
}

void
browser_t::notify(const uint32_t delay_ms)
{
    myassert(!notified_);
    notified_ = true;
    scheduleCallback(&notified, this, delay_ms);
}

void
browser_t::on_timeout_timer_fired(const uint32_t loadnum)
{
    logself(DEBUG, "begin");

    if (loadnum == loadnum_) {
        logself(DEBUG, "cancel current load");

        report_failed_load("timedout");
        stop_load();
        // immediately schedule the next load
        ++loadnum_;
        notify();
    } else {
        logself(DEBUG, "not the same loadnum -> do nothing");
    }

    logself(DEBUG, "done");
}

void
browser_t::on_notified()
{
    logself(DEBUG, "begin");

    myassert(notified_);
    notified_ = false;

    if (state == SB_CLOSED) {
        return;
    }
    if (state == SB_INIT) {

        // pick a random page to load
        page_specs_idx_ = rand() % page_specs_.size();

        expected_objects_ = page_specs_[page_specs_idx_]->get_expected_objects();
        load(page_specs_[page_specs_idx_]->url_);
        return;
    }

    if (state == SB_DONE_DOCUMENT) {
        if (doc_is_html_) {
            request_embedded_objects();

            if (is_page_done()) {
                logself(DEBUG, "no embedded resources -> done");
                state = SB_DONE;
            }
            else {
                logself(DEBUG, "go to state: fetching embedded resources");
                state = SB_FETCHING_EMBEDDED;
            }
        } else {
            logself(DEBUG, "main doc is not html -> done");
            state = SB_DONE;
        }
    }

    if (state == SB_DONE) {
        struct timeval t;
        myassert(0 == gettimeofday(&t, NULL));
        load_done_timepoint_ = gettimeofdayMs(&t);
        logself(DEBUG, "load_done_timepoint_ %d", load_done_timepoint_);

        verify_page_load();

        report_result();

        // this reset state to SB_INIT
        reset();

        // schedule next page load

        guint sleep_ms = 0;

        if (think_times_cdf) {
            const double percentile = (*think_time_rand_gen)();
            sleep_ms =
                (guint) (cdf_getValue(think_times_cdf, percentile));
        } else if (think_time_rand_gen) {
            sleep_ms = (guint)(*think_time_rand_gen)();
        }

        ++loadnum_;
        logself(DEBUG, "sleep_ms %u", sleep_ms);
        notify(sleep_ms);
    }

    logself(DEBUG, "done");
    return;
}

void
browser_t::validate_one_resource(const string& url,
                                 const size_t& actual_body_size,
                                 const char* actual_digest,
                                 map<string, ExpectedObj>& expected_objects)
{
    logself(DEBUG, "begin, validating resource [%s]... ", url.c_str());
    if (!inMap(expected_objects, url)) {
        validate_result_ = VR_FAIL;
        logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
              "error: did not expect resource [%s]", url.c_str());
        ++totalnumerrorobjects_;
    } else {
        const ExpectedObj& eo = (expected_objects)[url];
        if (actual_body_size != eo.bodySize) {
            logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
                  "error: resource [%s] expected body size= %d, actual= %d",
                  url.c_str(), eo.bodySize, actual_body_size);
            validate_result_ = VR_FAIL;
            ++totalnumerrorobjects_;
        } else if (eo.hexMd5Digest && actual_digest) {
            /* if the size differs we don't compare digests */
            if (strcmp(actual_digest, eo.hexMd5Digest->c_str())) {
                logfn(SHADOW_LOG_LEVEL_WARNING, __func__,
                      "error: resource [%s] expected digest= %s, actual= %s",
                      url.c_str(), eo.hexMd5Digest->c_str(), actual_digest);
                validate_result_ = VR_FAIL;
                ++totalnumerrorobjects_;
            }
        }
        expected_objects.erase(url);
    }
    logself(DEBUG, "new totalnumerrorobjects_ %u", totalnumerrorobjects_);
    logself(DEBUG, "done");
}

void
browser_t::verify_page_load()
{
    logself(DEBUG, "begin");

    /* as the resources are received, we already do the validation in
     * the body_done cb functions.
     *
     * if any such resource is extraneous (we did not expect it) or is
     * not as expected (size or digest difference), then we already
     * marked validate_result_ to fail.
     *
     * here we detect expected resources that were not received.
     */

    map<string, ExpectedObj>::const_iterator it = expected_objects_.begin();
    for (; it != expected_objects_.end(); ++it) {
        logself(WARNING, "error: did not receive resource [%s]",
                it->first.c_str());
        validate_result_ = VR_FAIL;
    }
    
    logself(DEBUG, "done");
}

void
browser_t::report_failed_load(const char *reason) const
{
    size_t totaltxbytes = 0, totalrxbytes = 0;

    connman_->get_total_bytes(totaltxbytes, totalrxbytes);
    char *s = NULL;
    asprintf(&s,
             "loadnum= %u, %s: FAILED: start= %" PRIu64 " reason= [%s] url= [%s] totalrxbytes= %zu",
             loadnum_,
             (do_spdy_ ? "spdy" : "vanilla"),
             load_start_timepoint_,
             reason,
             page_specs_[page_specs_idx_]->url_.c_str(),
             (totalrxbytes)
        );
    logfn(SHADOW_LOG_LEVEL_MESSAGE, __func__, "%s", s);
    free(s);
}

void
browser_t::report_result() const
{
    size_t totaltxbytes = 0, totalrxbytes = 0;

    myassert(load_done_timepoint_ > load_start_timepoint_);
    const uint64_t timestamp_recv_first_byte = connman_->get_timestamp_recv_first_byte();
    myassert(timestamp_recv_first_byte > load_start_timepoint_);

    connman_->get_total_bytes(totaltxbytes, totalrxbytes);
    char *s = NULL;
    asprintf(&s,
             "loadnum= %u, %s: %s: start= %" PRIu64 " plt= %" PRIu64 " url= [%s] ttfb= %" PRIu64 " totalbodybytes= %zu totaltxbytes= %zu totalrxbytes= %zu numobjects= %u numerrorobjects= %u",
             loadnum_,
             (do_spdy_ ? "spdy" : "vanilla"),
             (validate_result_ == VR_SUCCESS) ? "success" : "FAILED",
             load_start_timepoint_,
             (validate_result_ == VR_SUCCESS) ? (load_done_timepoint_ - load_start_timepoint_) : 0,
             page_specs_[page_specs_idx_]->url_.c_str(),
             (validate_result_ == VR_SUCCESS) ? (timestamp_recv_first_byte - load_start_timepoint_) : 0,
             (totalbodybytes_),
             (totaltxbytes),
             (totalrxbytes),
             (totalnumobjects_),
             (totalnumerrorobjects_));
    logfn(SHADOW_LOG_LEVEL_MESSAGE, __func__, "%s", s);
    free(s);
}

void
browser_t::close()
{
    logself(DEBUG, "begin");

    if (evbase_) {
        delete evbase_;
        evbase_ = NULL;
    }

    // kill all connections and requests
    connman_->reset();

    {
        map<string, Request*>::iterator it = pending_requests_.begin();
        for (; it != pending_requests_.end(); ++it) {
            delete it->second;
        }
        pending_requests_.clear();
    }
    
    state = SB_CLOSED;

    logself(DEBUG, "done");
}

void
browser_t::reset()
{
    // do not touch evbase_ and "config" stuff: the socks5
    // addr/port/host, page_specs_, think_times

    state = SB_INIT;
    first_hostname_.clear();

    {
        map<uintptr_t, EVP_MD_CTX*>::iterator it = req2mdctx.begin();
        for (; it != req2mdctx.end(); ++it) {
            if (it->second) {
                EVP_MD_CTX_destroy(it->second);
            }
        }
        req2mdctx.clear();
    }

    if (connman_) {
        connman_->reset();
    }

    myassert(pending_requests_.size() == 0);

    doc_is_html_ = false;
    doc_req_instNum_ = -1;
    doc_content.clear();
    doc_expected_len_ = 0;
    notified_ = false;
    validate_result_ = VR_NONE;
    totalnumerrorobjects_ = totalnumobjects_ = 0;
    totalbodybytes_ = 0;
    load_start_timepoint_ = load_done_timepoint_ = 0;
    received_resources_.clear();
    embedded_resources_.clear();
}

void
browser_t::stop_load()
{
    logself(DEBUG, "begin");

    {
        map<string, Request*>::iterator it = pending_requests_.begin();
        for (; it != pending_requests_.end(); ++it) {
            delete it->second;
        }
        pending_requests_.clear();
    }

    reset();

    logself(DEBUG, "done");
}

browser_t::PageSpec::~PageSpec()
{
    for (map<string, ExpectedObj>::iterator it = expected_objects_.begin();
         it != expected_objects_.end(); ++it)
    {
        if (it->second.hexMd5Digest) {
            delete it->second.hexMd5Digest;
            it->second.hexMd5Digest = NULL;
        }
    }
}

void
browser_t::PageSpec::add_expected_object(const std::string& url,
                                         const ExpectedObj& eo)
{
//    logDEBUG("url [%s] , before ___ size %d", url.c_str(), expected_objects_.size());
    expected_objects_[url] = eo;
//    logDEBUG("after ___ size %d", expected_objects_.size());
}
