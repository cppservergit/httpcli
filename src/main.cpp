#include "http_client.hpp"
#include <iostream>
#include <thread>
#include <vector>
#include <latch>

using net::HttpClient;
using net::HttpRequestException;

void run_multipart_test(std::latch& done) {
    HttpClient client;

    try {
        const std::string url = "https://httpbin.org/post";
        const std::map<std::string, std::string> fields = {
            { "field1", "value one" },
            { "field2", "value two" }
        };

        const std::map<std::string, std::string> files = {
            { "sample", "src/main.cpp" } // Upload this source file!
        };

        const auto response = client.post_multipart(url, fields, files, {
            { "Accept", "application/json" }
        });

        std::cout << "[MULTIPART] Status: " << response.status_code << '\n';
        std::cout << "[MULTIPART] Headers:\n";
        for (const auto& [key, value] : response.headers)
            std::cout << "  " << key << ": " << value << '\n';
        std::cout << "[MULTIPART] Body (truncated):\n"
                  << response.body.substr(0, 400) << "...\n";
    } catch (const HttpRequestException& ex) {
        std::cerr << "[MULTIPART] Error: " << ex.what() << '\n';
    }

    done.count_down();
}


void run_get_test(std::latch& done) {
    HttpClient client;

    try {
        const std::string url = "https://jsonplaceholder.typicode.com/posts";
        const auto response = client.get(url, {
            { "Accept", "application/json" }
        });

        std::cout << "[GET] Status: " << response.status_code << '\n';
        std::cout << "[GET] Headers:\n";
        for (const auto& [key, value] : response.headers)
            std::cout << "  " << key << ": " << value << '\n';
        std::cout << "[GET] Body:\n"
                  << response.body << "...\n\n";
    } catch (const HttpRequestException& ex) {
        std::cerr << "[GET] Error: " << ex.what() << '\n';
    }

    done.count_down();
}

void run_post_test(std::latch& done) {
    HttpClient client;

    try {
        const std::string url = "https://jsonplaceholder.typicode.com/posts";
        const std::string json_body = R"({
            "title": "unit test with jthread",
            "body": "powered by Martin's HTTP client",
            "userId": 77
        })";

        const auto response = client.post(url, json_body, {
            { "Content-Type", "application/json" },
            { "Accept", "application/json" }
        });

        std::cout << "[POST] Status: " << response.status_code << '\n';
        std::cout << "[POST] Headers:\n";
        for (const auto& [key, value] : response.headers)
            std::cout << "  " << key << ": " << value << '\n';
        std::cout << "[POST] Body:\n" << response.body << '\n';
    } catch (const HttpRequestException& ex) {
        std::cerr << "[POST] Error: " << ex.what() << '\n';
    }

    done.count_down();
}

int main() {
    std::latch done(3);
    std::vector<std::jthread> threads;

    threads.emplace_back(run_get_test, std::ref(done));
    threads.emplace_back(run_post_test, std::ref(done));
    threads.emplace_back(run_multipart_test, std::ref(done));

    done.wait();
    std::cout << "[OK] All HTTP tests completed.\n";
    return 0;
}

