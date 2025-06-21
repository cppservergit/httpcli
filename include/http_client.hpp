#pragma once

#include <string>
#include <string_view>
#include <map>
#include <memory>
#include <functional>

namespace net {

struct HttpResponse {
    long status_code{};
    std::string body;
    std::map<std::string, std::string, std::less<>> headers;
};

class HttpClient {
public:
    virtual ~HttpClient() = default;

    [[nodiscard]] virtual HttpResponse get(std::string_view url,
                                           const std::map<std::string, std::string, std::less<>>& headers = {}) const = 0;

    [[nodiscard]] virtual HttpResponse post(std::string_view url,
                                            std::string_view body,
                                            const std::map<std::string, std::string, std::less<>>& headers = {}) const = 0;

    static std::unique_ptr<HttpClient> create(long connect_timeout_ms = 2000,
                                              long response_timeout_ms = 5000,
                                              std::string_view cert_path = {},
                                              std::string_view key_path = {},
                                              std::string_view key_pass = {});
};

} // namespace net
