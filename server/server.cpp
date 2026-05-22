#include <iostream>
#include <thread>
#include <mutex>
#include <atomic>
#include <chrono>
#include "httplib.h"
#include "nlohmann/json.hpp"
#include "decodeQRcode.h"
#include <csignal>

using json = nlohmann::json;

std::mutex mtx;
std::string g_action = "stop";
double g_time = 0;
std::atomic<bool> running{true};
std::atomic<long long> last_qr_update_ms{0};  // Время последнего обновления QR
const long long QR_TIMEOUT_MS = 500;  // Считать QR потеренным если нет обновления > 500ms
int poll_interval = 50;  // ms между проверками состояния

std::string localhost = "0.0.0.0";

class State {
public:
    double angle;
    double dist_move;
    std::string action;
    double time;
    
    void run() {
        // Начинаем с checkAngle, а не с waiting
        curState = &State::checkAngle;
        
        while (running) {
            (this->*curState)();
            // Небольшая задержка, чтобы не спамить CPU
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void (State::*curState)() = &State::checkAngle;  // Начальное состояние
    
    State() {
        action = "stop";
        time = 0;
        angle = 0;
        dist_move = 0;
    }
    
    void checkAngle() {
        angle = QR::getAngle();
        
        if (abs(angle) >= 5.0) {
            curState = &State::goRotate;
        } else {
            curState = &State::checkDist;
        }
    }
    
    void checkDist() {
        dist_move = QR::getWay();
        
        if (dist_move >= 50.0) {
            curState = &State::goForward;
        } else {
            // Цель достигнута
            action = "stop";
            time = 0;
            curState = &State::waiting;
        }
    }
    
    void waiting() {
        {
            std::lock_guard<std::mutex> lock(mtx);
            g_action = action;
            g_time = time;
        }
        
        // Вместо долгого sleep, используем более короткие интервалы
        std::this_thread::sleep_for(std::chrono::milliseconds(poll_interval));
        
        // Переход к следующему состоянию
        curState = &State::checkAngle;
    }
    
    void goRotate() {
        std::lock_guard<std::mutex> lock(mtx);  
        
        // Вычисляем время поворота
        time = (abs(angle) * 2.5 >= 400 ? 400 : abs(angle) * 2.5);
        
        if (angle > 0) {
            action = "left";
        } else {
            action = "right";
        }
        
        curState = &State::waiting;
    }
    
    void goForward() {
        std::lock_guard<std::mutex> lock(mtx);
        action = "forward";
        time = 50;
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
    
    // Небольшая задержка, чтобы QR поток успел инициализироваться
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    std::thread logic_thread([&]() { state.run(); });
    
    svr.Get("/poll", [](const httplib::Request&, httplib::Response& res) {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()
        ).count();
        
        // Проверяем, свежие ли данные QR
        bool qr_valid = QR::checkAllQR && 
                        (now - last_qr_update_ms) < QR_TIMEOUT_MS;
        
        if (!qr_valid) {
            // Отправляем JSON с ошибкой вместо пустого ответа
            res.status = 503;
            json error_response;
            error_response["error"] = "QR codes not detected";
            error_response["action"] = "stop";
            error_response["time"] = 0;
            
            std::cout << "poll (503): QR codes not detected\n";
            res.set_content(error_response.dump(), "application/json");
            return;
        }
        
        // Берем текущую команду
        std::lock_guard<std::mutex> lock(mtx);
        json command;
        command["action"] = g_action;
        command["time"]   = g_time;
        
        // Сбрасываем команду
        g_action = "stop";
        g_time = 0;
        
        std::cout << "poll (200): " << command.dump() << "\n";
        res.set_content(command.dump(), "application/json");
    });
    
    // Отдельный поток для обновления времени последнего QR update
    std::thread qr_monitor([&]() {
        while (running) {
            if (QR::checkAllQR) {
                last_qr_update_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()
                ).count();
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    });
    
    std::cout << "Server started on port 8080\n";
    svr.listen(localhost, 8080);  // блокирует до svr.stop()
    
    running = false;
    logic_thread.join();
    qr_monitor.join();
    QR::close();
    QR_thread.join();
    
    return 0;
}