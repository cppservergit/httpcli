#include "http_client.hpp"
#include <iostream>
#include <future>
#include <vector>
#include <cstdlib>

int main() {
    std::vector<std::future<int>> tests;

    // Test 1: GET request to httpbin.org/get.
    tests.push_back(std::async(std::launch::async, []() -> int {
        int failures = 0;
        try {
            http::client cli;
            auto res = cli.get("https://httpbin.org/get", {"User-Agent: modern-cpp23-http-client"});
            std::cout << "[Test 1] GET https://httpbin.org/get => Status: " << res.status_code << "\n";
            if (res.status_code != 200)
                failures++;
        } catch (const std::exception &e) {
            std::cerr << "[Test 1] GET request failed: " << e.what() << "\n";
            failures++;
        }
        return failures;
    }));

    // Test 2: POST request with form data to httpbin.org/post.
    tests.push_back(std::async(std::launch::async, []() -> int {
        int failures = 0;
        try {
            http::client cli;
            std::string payload = "name=test&value=123";
            auto res = cli.post("https://httpbin.org/post", payload, {
                "Content-Type: application/x-www-form-urlencoded",
                "User-Agent: modern-cpp23-http-client"
            });
            std::cout << "[Test 2] POST https://httpbin.org/post => Status: " << res.status_code << "\n";
            if (res.status_code != 200)
                failures++;
        } catch (const std::exception &e) {
            std::cerr << "[Test 2] POST request failed: " << e.what() << "\n";
            failures++;
        }
        return failures;
    }));

    // Test 3: GET request to GitHub API.
    tests.push_back(std::async(std::launch::async, []() -> int {
        int failures = 0;
        try {
            http::client cli;
            auto res = cli.get("https://api.github.com", {"User-Agent: modern-cpp23-http-client"});
            std::cout << "[Test 3] GET https://api.github.com => Status: " << res.status_code << "\n";
            if (res.status_code != 200)
                failures++;
        } catch (const std::exception &e) {
            std::cerr << "[Test 3] GitHub API GET request failed: " << e.what() << "\n";
            failures++;
        }
        return failures;
    }));

    // Test 4: POST JSON to jsonplaceholder.
    tests.push_back(std::async(std::launch::async, []() -> int {
        int failures = 0;
        try {
            http::client cli;
            std::string json_payload = R"({
                "title": "foo",
                "body": "bar",
                "userId": 1
            })";
            auto res = cli.post("https://jsonplaceholder.typicode.com/posts", json_payload, {
                "Content-Type: application/json",
                "User-Agent: modern-cpp23-http-client"
            });
            std::cout << "[Test 4] POST JSON to jsonplaceholder => Status: " << res.status_code << "\n";
            if (res.status_code != 201)
                failures++;
            std::cout << "[Test 4] Response Body:\n" << res.body << "\n";
        } catch (const std::exception &e) {
            std::cerr << "[Test 4] JSON POST request failed: " << e.what() << "\n";
            failures++;
        }
        return failures;
    }));

    // Test 5: GET request to expired.badssl.com (invalid cert site).
    tests.push_back(std::async(std::launch::async, []() -> int {
        int failures = 0;
        try {
            http::client cli;
            // Invalid certificates are ignored by default.
            auto res = cli.get("https://expired.badssl.com/", {"User-Agent: modern-cpp23-http-client"});
            std::cout << "[Test 5] GET https://expired.badssl.com/ => Status: " << res.status_code << "\n";
            if (res.status_code != 200)
                failures++;
        } catch (const std::exception &e) {
            std::cerr << "[Test 5] GET request on expired.badssl.com failed: " << e.what() << "\n";
            failures++;
        }
        return failures;
    }));

    // Test 6: GET request and print response headers.
    tests.push_back(std::async(std::launch::async, []() -> int {
        int failures = 0;
        try {
            http::client cli;
            auto res = cli.get("https://httpbin.org/get", {"User-Agent: modern-cpp23-http-client"});
            std::cout << "[Test 6] GET https://httpbin.org/get (headers):\n";
            for (const auto &header : res.headers)
                std::cout << header.first << ": " << header.second << "\n";
            if (res.status_code != 200)
                failures++;
        } catch (const std::exception &e) {
            std::cerr << "[Test 6] GET (printing headers) failed: " << e.what() << "\n";
            failures++;
        }
        return failures;
    }));

    // Wait for all tests to complete and count failures.
    int total_failures = 0;
    for (auto &fut : tests) {
        total_failures += fut.get();
    }

    if (total_failures > 0) {
        std::cerr << total_failures << " test(s) failed.\n";
        return EXIT_FAILURE;
    }

    std::cout << "All tests passed successfully.\n";
    return EXIT_SUCCESS;
}
