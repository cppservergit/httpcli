#include "http_client.hpp"
#include <sstream>

namespace http {

// RAII helper for managing curl_slist resources.
struct CurlSListDeleter {
    void operator()(curl_slist* list) const {
        if (list) {
            curl_slist_free_all(list);
        }
    }
};
using CurlSListPtr = std::unique_ptr<curl_slist, CurlSListDeleter>;

// Define local type aliases (instead of using the private ones from client)
namespace {
    using ResponseStringPtr = std::string*;            // for response body
    using HeaderVectorPtr = std::vector<std::string>*;   // for headers

    // Helper functions to register our callbacks with libcurl.
    inline void setCurlWriteFunction(CURL* curl, std::string* responseData) {
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,
            reinterpret_cast<size_t(*)(char*, size_t, size_t, void*)>(client::write_callback));
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, static_cast<void*>(responseData));
    }

    inline void setCurlHeaderFunction(CURL* curl, std::vector<std::string>* headerData) {
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION,
            reinterpret_cast<size_t(*)(char*, size_t, size_t, void*)>(client::header_callback));
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, static_cast<void*>(headerData));
    }
} // unnamed namespace

// The constructor does not need to initialize ignore_invalid_certs_ explicitly,
// since it is in-class initialized to true.
client::client() {
    static const auto init_global = []() {
        curl_global_init(CURL_GLOBAL_ALL);
        std::atexit([](){ curl_global_cleanup(); });
        return 0;
    }();
    (void)init_global; // Suppress unused variable warning.
}

client::~client() = default;

void client::set_connection_timeout(long seconds) noexcept {
    connection_timeout_ = seconds;
}

void client::set_response_timeout(long seconds) noexcept {
    response_timeout_ = seconds;
}

void client::set_ignore_invalid_certs(bool ignore) noexcept {
    ignore_invalid_certs_ = ignore;
}

// Updated: "key" is passed by const reference.
void client::set_client_certificate(const std::string& cert,
                                      const std::optional<std::string>& key) noexcept {
    client_cert_ = cert;
    client_key_ = key;
}

client::response client::get(const std::string& url, const std::vector<std::string>& request_headers) {
    return perform_request("GET", url, "", request_headers);
}

client::response client::post(const std::string& url, const std::string& payload,
                                const std::vector<std::string>& request_headers) {
    return perform_request("POST", url, payload, request_headers);
}

// Callback now accepts a pointer-to-const.
size_t client::write_callback(const char* ptr, size_t size, size_t nmemb, void* userdata) {
    if (!userdata)
        return 0;
    std::string* out = static_cast<std::string*>(userdata);
    size_t total_size = size * nmemb;
    out->append(ptr, total_size);
    return total_size;
}

// Callback now accepts a pointer-to-const.
size_t client::header_callback(const char* buffer, size_t size, size_t nitems, void* userdata) {
    if (!userdata)
        return 0;
    std::vector<std::string>* headers = static_cast<std::vector<std::string>*>(userdata);
    size_t total_size = size * nitems;
    std::string header_line(buffer, total_size);
    while (!header_line.empty() && (header_line.back() == '\r' || header_line.back() == '\n'))
        header_line.pop_back();
    headers->push_back(header_line);
    return total_size;
}

curl_slist* client::prepare_headers(const std::vector<std::string>& headers) {
    curl_slist* list = nullptr;
    for (const auto& header : headers)
        list = curl_slist_append(list, header.c_str());
    return list;
}

std::string client::trim(const std::string& str) {
    const std::string whitespace = " \n\r\t\f\v";
    const auto start = str.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return "";
    const auto end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

std::vector<std::pair<std::string, std::string>> client::parse_headers(const std::vector<std::string>& raw_headers) {
    std::vector<std::pair<std::string, std::string>> headers;
    for (const auto& line : raw_headers) {
        auto pos = line.find(':');
        if (pos != std::string::npos) {
            std::string key = trim(line.substr(0, pos));
            std::string value = trim(line.substr(pos + 1));
            headers.emplace_back(std::move(key), std::move(value));
        }
    }
    return headers;
}

client::response client::perform_request(const std::string& method,
                                           const std::string& url,
                                           const std::string& payload,
                                           const std::vector<std::string>& req_headers) {
    response res;
    CURL* raw_curl = curl_easy_init();
    if (!raw_curl)
        throw std::runtime_error("Failed to initialize curl");

    auto curl_deleter = [](CURL* p) { if (p) curl_easy_cleanup(p); };
    std::unique_ptr<CURL, decltype(curl_deleter)> curl(raw_curl, curl_deleter);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    
    // Enforce TLS v1.2 and a strong cipher list.
    curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_CIPHER_LIST, "HIGH:!aNULL:!MD5");

    if (method == "POST") {
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    }

    if (connection_timeout_.has_value())
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, connection_timeout_.value());
    if (response_timeout_.has_value())
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, response_timeout_.value());

    std::string response_body;
    setCurlWriteFunction(curl.get(), &response_body);

    std::vector<std::string> raw_headers;
    setCurlHeaderFunction(curl.get(), &raw_headers);

    // Use RAII to manage the curl_slist.
    CurlSListPtr header_list_ptr(prepare_headers(req_headers));
    if (header_list_ptr)
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list_ptr.get());

    // If ignoring invalid certificates (includes expired certs), set verification off.
    if (ignore_invalid_certs_) {
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYPEER, 0L);
        curl_easy_setopt(curl.get(), CURLOPT_SSL_VERIFYHOST, 0L);
    }
    if (client_cert_.has_value()) {
        curl_easy_setopt(curl.get(), CURLOPT_SSLCERT, client_cert_->c_str());
        if (client_key_.has_value())
            curl_easy_setopt(curl.get(), CURLOPT_SSLKEY, client_key_->c_str());
    }

    CURLcode code = curl_easy_perform(curl.get());
    if (code != CURLE_OK)
        throw std::runtime_error("curl_easy_perform() failed: " + std::string(curl_easy_strerror(code)));

    long http_code = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &http_code);
    res.status_code = http_code;
    res.body = std::move(response_body);
    res.headers = parse_headers(raw_headers);

    return res;
}

} // namespace http
