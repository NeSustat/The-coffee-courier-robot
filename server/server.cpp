#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "decodeQRcode.h"
#include <csignal>

using json = nlohmann::json;

std::mutex mtx;
std::string g_action = "stop";
double g_time = 0;
std::atomic<bool> running{true};

std::string localhost = "0.0.0.0";

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
    void (State::*curState)() = &State::waiting;
    State(){
        angle = QR::getAngle();
        dist_move = QR::getWay();
    }
    void checkAngle(){
        angle = QR::getAngle();
        if (abs(angle) >= 5.0){
            curState = &State::goRotate;
        } else {
            curState = &State::checkDist;
        }
    }
    void checkDist(){
        dist_move = QR::getWay();
        std::cout << "DIST: " << dist_move << std::endl;
        if (dist_move >= 50.0){
            curState = &State::goForward;
        } else {
            action = "stop";
            time = 200;
            curState = &State::waiting;
        }
    }
    void waiting(){
        {
            std::lock_guard<std::mutex> lock(mtx);
            g_action = action;
            g_time = time;
            // std::cout << g_action << " " << time << std::endl;
        }
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
        curState = &State::waiting;
    }
    void goForward(){
        std::lock_guard<std::mutex> lock(mtx);
        action = "forward";
        time = 200;
        curState = &State::waiting;
    }
};

httplib::Server svr;

void signalHandler(int) {
    running = false;
    svr.stop();
    QR::close();
}

State state;

int main() {
    std::signal(SIGINT, signalHandler);

    std::thread QR_thread(QR::run);

    std::thread logic_thread([&]() { state.run(); });
    svr.Get("/poll", [](const httplib::Request&, httplib::Response& res) {
        if (!QR::checkAllQR) {
            res.status = 503;
            return;
        }
        std::lock_guard<std::mutex> lock(mtx);
        json command;
        command["action"] = g_action;
        command["time"]   = g_time;
        g_action = "stop";
        g_time = 200;
        std::cout << "poll: " << command.dump() << "\n";
        res.set_content(command.dump(), "application/json");
    });

    std::cout << "Server started on port 8080\n";
    svr.listen(localhost, 8080); // блокирует до svr.stop()

    running = false;
    logic_thread.join();
    QR::close();
    QR_thread.join();
    return 0;
}
