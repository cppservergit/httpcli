#include "http_client.hpp"
#include <sstream>

namespace http {

// Constructor: Initialize libcurl globally and register cleanup with atexit.
client::client() : ignore_invalid_certs_{true} {
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

void client::set_client_certificate(const std::string& cert, std::optional<std::string> key) noexcept {
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

// Callback that writes the response body.
size_t client::write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
    auto* stream = static_cast<std::string*>(userdata);
    size_t total_size = size * nmemb;
    stream->append(ptr, total_size);
    return total_size;
}

// Callback that collects raw header data.
size_t client::header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
    size_t total_size = size * nitems;
    auto* headers = static_cast<std::vector<std::string>*>(userdata);
    std::string header_line(buffer, total_size);
    // Remove trailing newline and carriage return characters.
    while (!header_line.empty() && (header_line.back() == '\r' || header_line.back() == '\n'))
        header_line.pop_back();
    headers->push_back(header_line);
    return total_size;
}

// Convert vector of header strings to a libcurl header list.
curl_slist* client::prepare_headers(const std::vector<std::string>& headers) {
    curl_slist* list = nullptr;
    for (const auto& header : headers)
        list = curl_slist_append(list, header.c_str());
    return list;
}

// Simple trimming helper.
std::string client::trim(const std::string& str) {
    const std::string whitespace = " \n\r\t\f\v";
    const auto start = str.find_first_not_of(whitespace);
    if (start == std::string::npos)
        return "";
    const auto end = str.find_last_not_of(whitespace);
    return str.substr(start, end - start + 1);
}

// Parse raw header lines into key-value pairs.
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

// Performs the HTTP request.
client::response client::perform_request(const std::string& method,
                                           const std::string& url,
                                           const std::string& payload,
                                           const std::vector<std::string>& req_headers) {
    response res;
    CURL* raw_curl = curl_easy_init();
    if (!raw_curl)
        throw std::runtime_error("Failed to initialize curl");

    // Wrap the CURL handle so it is automatically cleaned up.
    auto curl_deleter = [](CURL* p) { if (p) curl_easy_cleanup(p); };
    std::unique_ptr<CURL, decltype(curl_deleter)> curl(raw_curl, curl_deleter);

    curl_easy_setopt(curl.get(), CURLOPT_URL, url.c_str());
    
    // Enforce strong TLS version and ciphers:
    curl_easy_setopt(curl.get(), CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl.get(), CURLOPT_SSL_CIPHER_LIST, "HIGH:!aNULL:!MD5");

    // Configure HTTP method specifics.
    if (method == "POST") {
        curl_easy_setopt(curl.get(), CURLOPT_POST, 1L);
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS, payload.c_str());
        curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDSIZE, static_cast<long>(payload.size()));
    }

    // Set timeouts if specified.
    if (connection_timeout_.has_value())
        curl_easy_setopt(curl.get(), CURLOPT_CONNECTTIMEOUT, connection_timeout_.value());
    if (response_timeout_.has_value())
        curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, response_timeout_.value());

    // Set callbacks for response body and headers.
    std::string response_body;
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA, &response_body);

    std::vector<std::string> raw_headers;
    curl_easy_setopt(curl.get(), CURLOPT_HEADERFUNCTION, header_callback);
    curl_easy_setopt(curl.get(), CURLOPT_HEADERDATA, &raw_headers);

    // Set request headers if provided.
    curl_slist* header_list = prepare_headers(req_headers);
    if (header_list)
        curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, header_list);

    // Configure SSL options.
    // Invalid certificates are ignored by default.
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
    if (header_list)
        curl_slist_free_all(header_list);
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
