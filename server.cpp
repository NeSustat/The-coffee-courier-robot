#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include "httplib.h"
#include "nlohmann/json.hpp"

using json = nlohmann::json;

std::mutex mtx;
std::string g_action = "stop";
double g_time = 0;
std::atomic<bool> running{true};

int main() {
    httplib::Server svr;

    svr.Get("/poll", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        json command;
        command["action"] = g_action;
        command["time"]   = g_time;
        g_action = "";
        g_action = "";
        res.set_content(command.dump(), "application/json");
    });

    std::thread server_thread([&]() {
        svr.listen("localhost", 8080);
    });

    std::cout << "Commands: forward / left / right / stop / exit  +  time(ms)\n";
    while (running) {
        std::string action;
        double time;
        std::cin >> action >> time;

        if (action == "exit") {
            running = false;
            svr.stop(); // останавливает svr.listen() → server_thread завершается
            break;
        }

        std::lock_guard<std::mutex> lock(mtx);
        g_action = action;
        g_time   = time;
    }

    server_thread.join(); // ждём завершения потока и освобождаем ресурсы
    return 0;
}
