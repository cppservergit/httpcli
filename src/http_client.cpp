#include "http_client.hpp"
#include <curl/curl.h>
#include <stdexcept>
#include <sstream>
#include <map>
#include <mutex>

namespace net {

namespace {
    size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* buffer) {
        buffer->append(static_cast<char*>(contents), size * nmemb);
        return size * nmemb;
    }

    size_t header_callback(char* buffer, size_t size, size_t nitems, void* userdata) {
        auto* headers = static_cast<std::map<std::string, std::string>*>(userdata);
        std::string header_line(buffer, size * nitems);
        auto colon = header_line.find(':');
        if (colon != std::string::npos) {
            auto key = header_line.substr(0, colon);
            auto value = header_line.substr(colon + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
            headers->insert({ std::move(key), std::move(value) });
        }
        return nitems * size;
    }

    class CurlGlobal {
    public:
        CurlGlobal() { curl_global_init(CURL_GLOBAL_DEFAULT); }
        ~CurlGlobal() { curl_global_cleanup(); }
    };

    CurlGlobal curl_init_once;
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
                     const std::map<std::string, std::string>& headers = {}) const override {
        return perform_request("GET", url, "", headers);
    }

    HttpResponse post(std::string_view url,
                      std::string_view body,
                      const std::map<std::string, std::string>& headers = {}) const override {
        return perform_request("POST", url, body, headers);
    }

private:
    long connect_timeout_ms_;
    long response_timeout_ms_;
    std::string cert_path_;
    std::string key_path_;
    std::string key_pass_;

    HttpResponse perform_request(std::string_view method,
                                 std::string_view url,
                                 std::string_view body,
                                 const std::map<std::string, std::string>& headers) const {
        CURL* curl = curl_easy_init();
        if (!curl) throw std::runtime_error("Failed to initialize CURL");

        std::string response_data;
        std::map<std::string, std::string, std::less<>> response_headers;

        curl_easy_setopt(curl, CURLOPT_URL, url.data());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response_data);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, header_callback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &response_headers);
        curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, connect_timeout_ms_);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, response_timeout_ms_);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "modern-http-client/1.0");

        if (url.starts_with("https://")) {
            curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
            curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);

            if (!cert_path_.empty()) {
                curl_easy_setopt(curl, CURLOPT_SSLCERT, cert_path_.c_str());
                curl_easy_setopt(curl, CURLOPT_SSLKEY, key_path_.c_str());
                if (!key_pass_.empty()) {
                    curl_easy_setopt(curl, CURLOPT_KEYPASSWD, key_pass_.c_str());
                }
            }
        }

        struct curl_slist* chunk = nullptr;
        for (const auto& [key, val] : headers) {
            std::string header = key + ": " + val;
            chunk = curl_slist_append(chunk, header.c_str());
        }
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

        if (method == "POST") {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.data());
        }

        CURLcode res = curl_easy_perform(curl);
        long status = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

        curl_slist_free_all(chunk);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            throw std::runtime_error(curl_easy_strerror(res));
        }

        return HttpResponse{ status, std::move(response_data), std::move(response_headers) };
    }
};

std::unique_ptr<HttpClient> HttpClient::create(long connect_timeout_ms,
                                               long response_timeout_ms,
                                               std::string_view cert_path,
                                               std::string_view key_path,
                                               std::string_view key_pass) {
    return std::make_unique<CurlHttpClient>(connect_timeout_ms, response_timeout_ms,
                                            cert_path, key_path, key_pass);
}

} // namespace net
