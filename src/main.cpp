#include "http_client.hpp"
#include <iostream>
#include <string_view>
#include <thread>
#include <stop_token>

using namespace std::literals;
using net::HttpClient;
using net::HttpRequestException;

void test_get(std::stop_token) {
    try {
        const auto client = HttpClient::create(1500, 3000);
        const auto response = client->get("https://httpbin.org/get"sv, {
            { "Accept", "application/json" }
        });
        std::cout << "[GET] Status: " << response.status_code << '\n'
                  << "[GET] Body:\n" << response.body << '\n';
    } catch (const HttpRequestException& e) {
        std::cerr << "[GET] Request error: " << e.what() << '\n';
    }
}

void test_post(std::stop_token) {
    try {
        const auto client = HttpClient::create(1500, 3000);
        const auto response = client->post("https://httpbin.org/post"sv,
                                           R"({"hello":"world"})"sv, {
            { "Content-Type", "application/json" }
        });
        std::cout << "[POST] Status: " << response.status_code << '\n'
                  << "[POST] Body:\n" << response.body << '\n';
    } catch (const HttpRequestException& e) {
        std::cerr << "[POST] Request error: " << e.what() << '\n';
    }
}

int main() {
    std::jthread get_thread(test_get);
    std::jthread post_thread(test_post);
    return 0;
}
