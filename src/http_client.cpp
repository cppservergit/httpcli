#include "http_client.hpp"
#include <curl/curl.h>
#include <cctype>
#include <iostream>

namespace {
    struct CurlGlobal {
        CurlGlobal() { std::cout << curl_global_init(CURL_GLOBAL_DEFAULT); }
        ~CurlGlobal() { std::cout << curl_global_cleanup(); }
        CurlGlobal(const CurlGlobal&) = delete;
        CurlGlobal& operator=(const CurlGlobal&) = delete;
        CurlGlobal(CurlGlobal&&) = delete;
        CurlGlobal& operator=(CurlGlobal&&) = delete;
    };

    const CurlGlobal curl_init_once{};
}

namespace net {

namespace {

	struct CurlCallbackContext {
		std::string body;
		std::map<std::string, std::string, std::less<>> headers;

		// NOSONAR - libcurl requires void* signature
		static size_t Write(const char* contents, size_t size, size_t nmemb, void* user_data) {
			auto* ctx = static_cast<CurlCallbackContext*>(user_data);
			ctx->body.append(contents, size * nmemb);
			return size * nmemb;
		}

		// NOSONAR - libcurl requires void* signature
		static size_t Header(const char* buffer, size_t size, size_t nitems, void* user_data) {
			auto* ctx = static_cast<CurlCallbackContext*>(user_data);
			const std::string header_line(buffer, size * nitems);
			if (const auto colon = header_line.find(':'); colon != std::string::npos) {
				auto key = header_line.substr(0, colon);
				auto value = header_line.substr(colon + 1);

				// Trim leading whitespace
				while (!value.empty() && std::isspace(static_cast<unsigned char>(value.front()))) {
					value.erase(value.begin());
				}

				// Trim trailing whitespace
				while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) {
					value.pop_back();
				}

				ctx->headers.try_emplace(std::move(key), std::move(value));
			}
			return size * nitems;
		}

	};


} // namespace

HttpClient::HttpClient() = default;
HttpClient::HttpClient(Config config) : config_{std::move(config)} {}

HttpClient HttpClient::with(const Config& config) {
    return HttpClient{config};
}

HttpClient::Response HttpClient::get(std::string_view url,
                                     const std::map<std::string, std::string, std::less<>>& headers) const {
    return post(url, {}, headers); // Shared logic supports GET and POST
}

HttpClient::Response HttpClient::post(std::string_view url,
                                      std::string_view body,
                                      const std::map<std::string, std::string, std::less<>>& headers) const {
    const auto curl = curl_easy_init();
    if (!curl)
        throw HttpRequestException("Failed to initialize CURL");

    CurlCallbackContext context;

    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_USE_SSL, CURLUSESSL_ALL);
    curl_easy_setopt(curl, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1_2);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlCallbackContext::Write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CurlCallbackContext::Header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config_.connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.response_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "modern-http-client/1.0");

    if (config_.cert_path)
        curl_easy_setopt(curl, CURLOPT_SSLCERT, config_.cert_path->c_str());
    if (config_.key_path)
        curl_easy_setopt(curl, CURLOPT_SSLKEY, config_.key_path->c_str());
    if (config_.key_pass)
        curl_easy_setopt(curl, CURLOPT_KEYPASSWD, config_.key_pass->c_str());

    struct curl_slist* chunk = nullptr;
    for (const auto& [key, val] : headers) {
        const std::string header = key + ": " + val;
        chunk = curl_slist_append(chunk, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    if (!body.empty()) {
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

    return { status, std::move(context.body), std::move(context.headers) };
}

HttpClient::Response HttpClient::post_multipart(std::string_view url,
                                                const std::map<std::string, std::string>& fields,
                                                const std::map<std::string, std::string>& files,
                                                const std::map<std::string, std::string, std::less<>>& headers) const {
    const auto curl = curl_easy_init();
    if (!curl)
        throw HttpRequestException("Failed to initialize CURL");

    CurlCallbackContext context;
    curl_easy_setopt(curl, CURLOPT_URL, url.data());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlCallbackContext::Write);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &context);
    curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, CurlCallbackContext::Header);
    curl_easy_setopt(curl, CURLOPT_HEADERDATA, &context);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, config_.connect_timeout_ms);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, config_.response_timeout_ms);

    // Apply SSL if needed
    if (config_.cert_path)
        curl_easy_setopt(curl, CURLOPT_SSLCERT, config_.cert_path->c_str());
    if (config_.key_path)
        curl_easy_setopt(curl, CURLOPT_SSLKEY, config_.key_path->c_str());
    if (config_.key_pass)
        curl_easy_setopt(curl, CURLOPT_KEYPASSWD, config_.key_pass->c_str());

    // Set up MIME form
    curl_mime* mime = curl_mime_init(curl);
    curl_mimepart* part;

    for (const auto& [name, value] : fields) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, name.c_str());
        curl_mime_data(part, value.c_str(), CURL_ZERO_TERMINATED);
    }

    for (const auto& [name, path] : files) {
        part = curl_mime_addpart(mime);
        curl_mime_name(part, name.c_str());
        curl_mime_filedata(part, path.c_str());
    }

    curl_easy_setopt(curl, CURLOPT_MIMEPOST, mime);

    // Optional headers (e.g. Authorization)
    struct curl_slist* chunk = nullptr;
    for (const auto& [key, val] : headers) {
        const std::string header = key + ": " + val;
        chunk = curl_slist_append(chunk, header.c_str());
    }
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, chunk);

    if (const auto res = curl_easy_perform(curl); res != CURLE_OK) {
        curl_slist_free_all(chunk);
        curl_mime_free(mime);
        curl_easy_cleanup(curl);
        throw HttpRequestException(curl_easy_strerror(res));
    }

    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);

    curl_slist_free_all(chunk);
    curl_mime_free(mime);
    curl_easy_cleanup(curl);

    return { status, std::move(context.body), std::move(context.headers) };
}


} // namespace net
