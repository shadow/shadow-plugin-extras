#include <glib.h>
#include <glib/gprintf.h>
#include "myassert.h"
#include <time.h>
#include <shd-library.h>

#include "request.hpp"
#include "common.hpp"

using std::vector;
using std::pair;
using std::string;

#ifdef ENABLE_MY_LOG_MACROS
/* "inst" stands for instance, as in, instance of a class */
#define loginst(level, inst, fmt, ...)                                  \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, req= %d): " fmt,  \
              __LINE__, (inst)->instNum_, ##__VA_ARGS__);               \
    } while (0)

/* like loginst, but shortcut "this" as instance */
#define logself(level, fmt, ...)                                        \
    do {                                                                \
        logfn(SHADOW_LOG_LEVEL_##level, __func__, "(ln %d, req= %d): " fmt,  \
              __LINE__, (this)->instNum_, ##__VA_ARGS__);               \
    } while (0)

#else
/* no-ops */
#define loginst(level, inst, fmt, ...)

#define logself(level, fmt, ...)

#endif

extern ShadowLogFunc logfn;

uint32_t Request::nextInstNum = 0;

Request::Request(
    const string& path, const string& host, const uint16_t& port, const string& url,
    RequestAboutToSendCb req_about_to_send_cb,
    ResponseMetaCb rsp_meta_cb, ResponseBodyDataCb rsp_body_data_cb,
    ResponseBodyDoneCb rsp_body_done_cb
    )
    : instNum_(nextInstNum)
    , path_(path), host_(host), port_(port), url_(url)
    , req_about_to_send_cb_(req_about_to_send_cb)
    , rsp_meta_cb_(rsp_meta_cb), rsp_body_data_cb_(rsp_body_data_cb)
    , rsp_body_done_cb_(rsp_body_done_cb)
    , conn(NULL), num_retries_(0), first_byte_pos_(0), body_size_(0)
{
    ++nextInstNum;
    loginst(DEBUG, this, "a new request url [%s]", url.c_str());
    myassert(host_.length() > 0);
}

Request::~Request()
{
    logself(DEBUG, "request destructor");
    conn = NULL;
}

static void
delete_req(void *ptr)
{
    delete ((Request*)ptr);
}

void
Request::deleteLater(ShadowCreateCallbackFunc scheduleCallback)
{
    scheduleCallback(delete_req, this, 0);
}

void
Request::add_header(const char* name, const char* value)
{
    logself(DEBUG, "adding header name= [%s], value= [%s]", name, value);
    headers_.push_back(std::make_pair(name, value));
}

void
Request::dump_debug() const
{
    logDEBUG("dumping request to log:");
    logDEBUG("  path: %s", path_.c_str());
    logDEBUG("  host: %s", host_.c_str());
    logDEBUG("  url: %s", url_.c_str());
}
