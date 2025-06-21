#include "http_client.hpp"
#include <iostream>
#include <thread>

void test_get(const net::HttpClient& client) {
    try {
        auto res = client.get("https://httpbin.org/get", { {"Accept", "application/json"} });
        std::cout << "GET Status: " << res.status_code << "\n";
        std::cout << "GET Body: " << res.body << "\n";
    } catch (const std::exception& e) {
        std::cerr << "GET Error: " << e.what() << "\n";
    }
}

void test_post(const net::HttpClient& client) {
    try {
        auto res = client.post("https://httpbin.org/post", R"({"msg":"hi"})",
                               { {"Content-Type", "application/json"} });
        std::cout << "POST Status: " << res.status_code << "\n";
        std::cout << "POST Body: " << res.body << "\n";
    } catch (const std::exception& e) {
        std::cerr << "POST Error: " << e.what() << "\n";
    }
}

int main() {
    auto client = net::HttpClient::create(1500, 4000); // 1.5s connect, 4s response

    std::thread t1(test_get, std::ref(*client));
    std::thread t2(test_post, std::ref(*client));

    t1.join();
    t2.join();
}
