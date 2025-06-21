#ifndef HTTP_CLIENT_HPP
#define HTTP_CLIENT_HPP

#include <curl/curl.h>
#include <string>
#include <vector>
#include <optional>
#include <stdexcept>
#include <memory>
#include <cstdlib>

namespace http {

class client {
public:
    // Explicit constructor to ensure there's no unintended implicit construction.
    explicit client();
    client(const client&) = default;
    client(client&&) noexcept = default;
    client& operator=(const client&) = default;
    client& operator=(client&&) noexcept = default;
    ~client();

    // Timeout setters.
    void set_connection_timeout(long seconds) noexcept;
    void set_response_timeout(long seconds) noexcept;

    // SSL options.
    // This client ignores invalid certificates by default.
    void set_ignore_invalid_certs(bool ignore) noexcept;
    void set_client_certificate(const std::string& cert, std::optional<std::string> key = std::nullopt) noexcept;

    // Response type.
    struct response {
        long status_code;
        std::string body;
        std::vector<std::pair<std::string, std::string>> headers;
    };

    // HTTP methods.
    response get(const std::string& url, const std::vector<std::string>& request_headers = {});
    response post(const std::string& url, const std::string& payload, const std::vector<std::string>& request_headers = {});

private:
    // Member variables.
    bool ignore_invalid_certs_;
    std::optional<std::string> client_cert_;
    std::optional<std::string> client_key_;
    std::optional<long> connection_timeout_;
    std::optional<long> response_timeout_;

    // Internal method for performing HTTP requests.
    response perform_request(const std::string& method,
                             const std::string& url,
                             const std::string& payload,
                             const std::vector<std::string>& req_headers);

    // Helper functions.
    static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata);
    static size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata);
    static curl_slist* prepare_headers(const std::vector<std::string>& headers);
    static std::string trim(const std::string& str);
    static std::vector<std::pair<std::string, std::string>> parse_headers(const std::vector<std::string>& raw_headers);
};

} // namespace http

#endif // HTTP_CLIENT_HPP
