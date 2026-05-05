#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "decodeQRcode.cpp"

using json = nlohmann::json;

std::mutex mtx, mtxQR;
std::string g_action = "stop";
double g_time = 0;
std::atomic<bool> running{true};

class State{
public:
    double angle;
    double dist_move;
    std::string action;
    double time;
    void run() {
        while (running)
            (this->*curState)();
    }
    void (State::*curState)() = &State::checkAngle;
    State(){
        angle = getAngle();
        dist_move = getWay();
    }
    void checkAngle(){
        angle = getAngle();
        if (abs(angle) >= 5.0){
            curState = &State::goRotate;
        } else {
            curState = &State::checkDist;
        }
    }
    void checkDist(){
        dist_move = getWay();
        if (dist_move <= 200.0){
            curState = &State::goForward;
        } else {
            action = "stop";
            time = 200;
            curState = &State::wating;
        }
    }
    void wating(){
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        curState = &State::checkAngle;
    }
    void goRotate(){
        std::lock_guard<std::mutex> lock(mtx);  
        time = 200;
        if (angle > 0){
            action = "left";
        } else {
            action = "right";
        }
        curState = &State::wating;
    }
    void goForward(){
        std::lock_guard<std::mutex> lock(mtx);
        action = "forward";
        time = 200;
        curState = &State::wating;
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
        State state;
        std::thread logic_thread([&]() { state.run(); });


        if (state.action == "exit") {
            running = false;
            svr.stop(); // останавливает svr.listen() → server_thread завершается
            break;
        }

        std::lock_guard<std::mutex> lock(mtx);
        g_action = state.action;
        g_time   = state.time;
    }

    server_thread.join(); // ждём завершения потока и освобождаем ресурсы
    return 0;
}
