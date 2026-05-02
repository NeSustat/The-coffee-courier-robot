#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "decodeQRcode.cpp"

using json = nlohmann::json;

std::mutex mtx;
std::string g_action = "stop";
double g_time = 0;
std::atomic<bool> running{true};

class State{
    double angle;
    double dist_move;
    std::string action;
    httplib::Server svr;
    double time;
    void (State::*curState)();
    State(){
        angle = getAngle();
        dist_move = getWay();
    }
    void checkAngle(){
        if (abs(angle) >= 5.0){
            curState = &goRotate;
        } else {
            curState = &checkDist;
        }
    }
    void checkDist(){
        if (dist_move <= 200.0){
            curState = &goForward;
        } else {
            curState = &wating;
        }
    }
    void wating(){
        svr.Get("/poll", [&](const httplib::Request&, httplib::Response& res) {
            std::lock_guard<std::mutex> lock(mtx);
            json command;
            command["action"] = action;
            command["time"]   = time;
            res.set_content(command.dump(), "application/json");
        });
        curState = &checkAngle;
    }
    void goRotate(){
        time = 200;
        if ()
        action = "left";
        curState = &checkDist;
    }
};

int main() {
    httplib::Server svr;

    svr.Get("/poll", [](const httplib::Request&, httplib::Response& res) {
        std::lock_guard<std::mutex> lock(mtx);
        json command;
        command["action"] = g_action;
        command["time"]   = g_time;
        g_action = "stop";
        g_time = 200;
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
