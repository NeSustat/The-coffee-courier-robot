#include <iostream>
#include <cmath>
#include "httplib.h"
#include <nlohmann/json.hpp>
#include <thread>

using json = nlohmann::json;

constexpr const char* SERVER_HOST = "127.0.0.1"; // IP ноутбука
constexpr int SERVER_PORT = 8080;
constexpr const char* POLL_PATH = "/poll";
constexpr int CONNECT_TIMEOUT_SEC = 1;
constexpr int READ_TIMEOUT_SEC   = 2;
constexpr int POLL_INTERVAL_MS   = 200;


void thr(int time_ms){

}

class AEngine{
public:
    void forward(int time_ms);
    void left(int time_ms);
    void right(int time_ms);
    void stop();
};

class FooEngine : AEngine{
public:
    void forward(int time_ms){
        std::cout << "robot move\n\n";
        thr(time_ms);
    }
    
    void left(int time_ms){
        std::cout << "robot turns to the left\n\n";
        thr(time_ms);
    }
    
    void right(int time_ms){
        std::cout << "robot turns to the right\n\n";
        thr(time_ms);
    }

    void stop(){
        std::cout << "power off\n\n";
        
    }
};

int main(){
    httplib::Client client("localhost", 8080);
    client.set_connection_timeout(CONNECT_TIMEOUT_SEC, 0);
    client.set_read_timeout(READ_TIMEOUT_SEC, 0);

    FooEngine robot;
    
    while (true) {
        auto res = client.Get(POLL_PATH);
        if (res && res->status == 200) {
            auto command = json::parse(res->body);
            std::string action = command["action"];
            int time = command["time"];
            std::cout << "action: " << action << ", time: " << time << "\n";
            if (action == "forward") {
                robot.forward(time);
            } else if (action == "left") {
                robot.left(time);
            } else if (action == "right") {
                robot.right(time);
            } else if (action == "stop") {
                robot.stop();
            }
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(POLL_INTERVAL_MS));
    }

}