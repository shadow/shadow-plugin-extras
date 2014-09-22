#ifndef SHD_REQUEST_HPP
#define SHD_REQUEST_HPP

#include <string>
#include <vector>

class Connection;
class Request;

typedef void (*ResponseMetaCb)(
    const int status, char **headers, Request* req, void* cb_data);
/* tell the user of a block of response body data */
typedef void (*ResponseBodyDataCb)(
    const uint8_t *data, const size_t& len, Request* req, void* cb_data);
/* tell the user there will be no more response body data */
typedef void (*ResponseBodyDoneCb)(Request *req, void* cb_data);
/* tell the user the request is about to be sent into the network */
typedef void (*RequestAboutToSendCb)(Request *req, void* cb_data);


class Request {
public:
    Request(const std::string& path, const std::string& host, const uint16_t& port,
            const std::string& url,
            RequestAboutToSendCb req_about_to_send_cb,
            ResponseMetaCb rsp_meta_cb, ResponseBodyDataCb rsp_body_data_cb,
            ResponseBodyDoneCb rsp_body_done_cb, void *cb_data
        );
    ~Request();

    /* schedule this cnx for later deletion */
    void deleteLater(ShadowCreateCallbackFunc scheduleCallback);

    void add_header(const char* name, const char* value);

    // for response
    void notify_rsp_meta(const int status, char ** headers) {
        rsp_meta_cb_(status, headers, this, cb_data_);
    }
    void notify_rsp_body_data(const uint8_t *data, const size_t& len) {
        body_size_ += len;
        rsp_body_data_cb_(data, len, this, cb_data_);
    }
    void notify_rsp_body_done() {
        rsp_body_done_cb_(this, cb_data_);
    }
    void notify_req_about_to_send() {
        if (req_about_to_send_cb_) {
            req_about_to_send_cb_(this, cb_data_);
        }
    }
    const std::string& get_path() const { return path_; }
    const size_t& get_body_size() const { return body_size_; }
    void dump_debug() const;
    const std::vector<std::pair<std::string, std::string> > & get_headers() const {
        return headers_;
    }

    size_t get_first_byte_pos() const { return first_byte_pos_; }
    void set_first_byte_pos(const size_t& pos) { first_byte_pos_ = pos; }

    int32_t get_num_retries() const { return num_retries_; }
    void increment_num_retries() { ++num_retries_; }

    const uintptr_t instNum_; // monotonic id of this instance

    // these are const, so ok to expose
    const std::string path_;
    const std::string host_; /* for host header */
    const uint16_t port_;
    const std::string url_;
    /* the cnx handling this req. currently Request class is not doing
     * anything with this pointer; it's here only for convenience of
     * other code */
    Connection* conn;

private:
    static uint32_t nextInstNum;

    std::vector<std::pair<std::string, std::string> > headers_;

    RequestAboutToSendCb req_about_to_send_cb_;
    ResponseMetaCb rsp_meta_cb_;
    ResponseBodyDataCb rsp_body_data_cb_;
    ResponseBodyDoneCb rsp_body_done_cb_;
    void* cb_data_;

    /* for Range requests. currently only support first_byte_pos. will
     * always include a range.
     */
    size_t first_byte_pos_;

    uint8_t num_retries_;

    size_t body_size_;
};

#endif /* SHD_REQUEST_HPP */
