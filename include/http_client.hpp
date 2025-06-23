#pragma once

#include <string>
#include <string_view>
#include <map>
#include <optional>
#include <stdexcept>

namespace net {

class HttpRequestException : public std::runtime_error {
public:
    explicit HttpRequestException(std::string_view message)
        : std::runtime_error(std::string(message)) {}
};

class HttpClient {
public:
    struct Config {
        long connect_timeout_ms = 2000;
        long response_timeout_ms = 5000;
        std::optional<std::string> cert_path;
        std::optional<std::string> key_path;
        std::optional<std::string> key_pass;
    };

    struct Response {
        long status_code{};
        std::string body;
        std::map<std::string, std::string, std::less<>> headers;
    };

    HttpClient();  // default config
    explicit HttpClient(Config config);
    static HttpClient with(const Config& config);

    [[nodiscard]] Response get(std::string_view url,
                               const std::map<std::string, std::string, std::less<>>& headers = {}) const;

    [[nodiscard]] Response post(std::string_view url,
                                std::string_view body,
                                const std::map<std::string, std::string, std::less<>>& headers = {}) const;

	Response post_multipart(std::string_view url,
							const std::map<std::string, std::string>& fields,
							const std::map<std::string, std::string>& files,
							const std::map<std::string, std::string, std::less<>>& headers = {}) const;

private:
    Config config_;
};

} // namespace net
