#include "HttpRouter.hpp"

#include <iostream>
#include <chrono>

static inline uint64_t rdtsc() {
    uint32_t lo, hi;
    asm volatile ("rdtsc" : "=a" (lo), "=d" (hi));
    return ((uint64_t)hi << 32) | lo;
}

static
void calculate_cpu_clock_speed() {
    uint64_t start, end;
    std::chrono::duration<double> elapsed;

    // Get TSC and time at start
    start = rdtsc();
    auto start_time = std::chrono::high_resolution_clock::now();

    // Busy-wait loop (~1 ms)
    do {
      auto end_time = std::chrono::high_resolution_clock::now();
      elapsed = end_time - start_time;
    } while (elapsed.count() < 0.001);

    end = rdtsc();

    double fcpu_hz = (end - start) / elapsed.count();
    double fcpu_ghz = fcpu_hz / 1e9;

    std::cout << "CPU freq: " << fcpu_ghz << " GHz\n";
}

void benchmark_routes() {
    struct UserData {
        int routed = 0;
    } userData;

    HttpRouter<UserData *> r;

    // set up a few routes
    r.add("GET", "/service/candy/:kind", [](UserData *user, auto &args) {
        user->routed++;
    });

    r.add("GET", "/service/shutdown", [](UserData *user, auto &args) {
        user->routed++;
    });

    r.add("GET", "/", [](UserData *user, auto &args) {
        user->routed++;
    });

    r.add("GET", "/:filename", [](UserData *user, auto &args) {
        user->routed++;
    });

    // run benchmark of various urls
    std::vector<std::string> test_urls = {
        "/service/candy/lollipop",
        "/service/candy/gum",
        "/service/candy/seg_råtta",
        "/service/candy/lakrits",

        "/service/shutdown",
        "/",
        "/some_file.html",
        "/another_file.jpeg"
    };

    for (std::string &test_url : test_urls) {
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 10000000; i++) {
            r.route("GET", 3, test_url.data(), test_url.length(), &userData);
        }
        auto stop = std::chrono::high_resolution_clock::now();
        unsigned int ms = std::chrono::duration_cast<std::chrono::milliseconds>(stop - start).count();
        std::cout << "[" << 10000.0 / ms << " million req/sec] for URL: " << test_url << std::endl;
    }

    std::cout << "Checksum: " << userData.routed << std::endl << std::endl;
}

void demo_routes() {
    struct UserData {
        // pass whatever you need as user data
    } userData;

    HttpRouter<UserData *> r;

    // set up a few routes
    r.add("GET", "/service/candy/:kind", [](UserData *user, auto &args) {
        std::cout << "Now serving candy of kind " << args[0] << std::endl;
    });

    r.add("GET", "/service/shutdown", [](UserData *user, auto &args) {
        std::cout << "Shutting down now" << std::endl;
    });

    r.add("GET", "/", [](UserData *user, auto &args) {
        std::cout << "Serving index now" << std::endl;
    });

    r.add("GET", "/:filename", [](UserData *user, auto &args) {
        std::cout << "Serving file: " << args[0] << std::endl;
    });

    r.add("GET", "/:page/:username", [](UserData *user, auto &args) {
        std::cout << "Serving page: " << args[0] << " username: " << args[1] << std::endl;
    });

    r.add("GET", "/service/:kind/dash/:type", [](UserData *user, auto &args) {
        std::cout << "Serving kind: " << args[0] << " type: " << args[1] << std::endl;
    });

    r.add("GET", "/service/:name/query/:querystr", [](UserData *user, auto &args) {
        std::cout << "Serving sevice name: " << args[0] << " query: " << args[1] << std::endl;
    });

    r.add("GET", "/service/*/logs", [](UserData *user, auto &args) {
        std::cout << "Now serving logs" << std::endl;
    });

    // run benchmark of various urls
    std::vector<std::string> test_urls = {
        "/service/candy/lollipop",
        "/service/candy/gum",
        "/service/candy/seg_råtta",
        "/service/candy/lakrits",

        "/service/shutdown",
        "/",
        "/some_file.html",
        "/another_file.jpeg",

        "/1/admin/",
        "/service/cheese/dash/mozarella",
        "/service/cheese/query/name/",
        "/service/mail/logs",
        "/service/upkeep/logs/?time=1732666926"
    };

    for (std::string &test_url : test_urls) {
        std::cout << "[" << test_url << "]" << std::endl;
        r.route("GET", 3, test_url.data(), test_url.length(), &userData);
    }
}

int main() {
    calculate_cpu_clock_speed();
    std::cout << "\nDemo\n\n";
    demo_routes();
    std::cout << "\nBenchmark\n\n";
    benchmark_routes();
}
