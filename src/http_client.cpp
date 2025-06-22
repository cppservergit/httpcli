#include "http_client.hpp"
#include <curl/curl.h>
#include <sstream>
#include <cctype>

namespace net {

namespace {

struct CurlCallbackContext {
    std::string body;
    std::map<std::string, std::string, std::less<>> headers;

    static size_t Write(char* contents, size_t size, size_t nmemb, void* user_data) {
        auto* ctx = static_cast<CurlCallbackContext*>(user_data);
        ctx->body.append(contents, size * nmemb);
        return size * nmemb;
    }

    static size_t Header(char* buffer, size_t size, size_t nitems, void* user_data) {
        auto* ctx = static_cast<CurlCallbackContext*>(user_data);
        const std::string header_line(buffer, size * nitems);
        if (const auto colon = header_line.find(':'); colon != std::string::npos) {
            auto key = header_line.substr(0, colon);
            auto value = header_line.substr(colon + 1);
            while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
                value.erase(value.begin());
            }
            ctx->headers.insert({ std::move(key), std::move(value) });
        }
        return size * nitems;
    }
};

class CurlGlobal {
public:
    CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
    ~CurlGlobal() { curl_global_cleanup(); }

    CurlGlobal(const CurlGlobal&) = delete;
    CurlGlobal& operator=(const CurlGlobal&) = delete;
    CurlGlobal(CurlGlobal&&) = delete;
    CurlGlobal& operator=(CurlGlobal&&) = delete;
};

const CurlGlobal curl_init_once;

} // namespace

class CurlHttpClient : public HttpClient {
public:
    CurlHttpClient(long connect_timeout_ms,
                   long response_timeout_ms,
                   std::string_view cert_path,
                   std::string_view key_path,
                   std::string_view key_pass)
        : connect_timeout_ms_{connect_timeout_ms},
          response_timeout_ms_{response_timeout_ms},
          cert_path_{cert_path},
          key_path_{key_path},
          key_pass_{key_pass} {}

    HttpResponse get(std::string_view url,
                     const std::map<std::string, std::string, std::less<>>& headers) const override {
        return perform_request("GET", url, {}, headers);
    }

    HttpResponse post(std::string_view url,
                      std::string_view body,
                      const std::map<std::string, std::string, std::less<>>& headers) const override {
        return perform_request("POST", url, body, headers);
    }

private:
    const long connect_timeout_ms_;
    const long response_timeout_ms_;
    const std::string cert_path_;
    const std::string key_path_;
    const std::string key_pass_;

    HttpResponse perform_request(std::string_view method,
                                 std::string_view url,
                                 std::string_view body,
                                 const std::map<std::string, std::string, std::less<>>& headers) const {
        const auto curl = curl_easy_init();
        if (!curl) {
            throw HttpRequestException("Failed to initialize CURL");
        }

        CurlCallbackContext context;

        curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
        curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
        curl_easy_setopt(curl, CURLOPT_URL, url.data());

        if (!cert_path_.empty()) {
            curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path_.c_str());
            curl_easy_setopt(curl, CURLOPT_SSLKEY, key_path_.c_str());
            if (!key_pass_.empty()) {
                curl_easy_setopt(curl, CURLOPT_KEYPASSWD, key_pass_.c_str());
            }
        }

        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlCallbackContext::Write);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CurlCallbackContext::Header);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms_);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, response_timeout_ms_);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "modern-http-client/1.0");

        struct curl_slist* chunk = nullptr;
        for (const auto& [key, val] : headers) {
            const std::string header = key + ": " + val;
            chunk = curl_slist_append(chunk, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        }

        if (const auto res = curl_easy_perform(curl); res != CURLE_OK) {
            curl_slist_free_all(chunk);
            curl_easy_cleanup(curl);
            throw HttpRequestException(curl_easy_strerror(res));
        }

        long status{};
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);

        return HttpResponse{ status, std::move(context.body), std::move(context.headers) };
    }
};

std::unique_ptr<HttpClient> HttpClient::create(long connect_timeout_ms,
                                               long response_timeout_ms,
                                               std::string_view cert_path,
                                               std::string_view key_path,
                                               std::string_view key_pass) {
    return std::make_unique<CurlHttpClient>(connect_timeout_ms,
                                            response_timeout_ms,
                                            cert_path, key_path, key_pass);
}

} // namespace net
