#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <queue>
#include <mutex>
#include <thread>
#include <atomic>

// Состояния конечного автомата
enum class State {
    IDLE,
    DETECTING,
    CALCULATING,
    MOVING,
    COMPLETED
};

// Команда для робота
struct Command {
    std::string action;  // "forward", "left", "right"
    int time_ms;
    
    Command() : action("stop"), time_ms(0) {}
    Command(const std::string& a, int t) : action(a), time_ms(t) {}
};

// Информация о цели
struct TargetInfo {
    std::string name;
    double angle;
    double distance;
    bool detected;
    cv::Point2f center;
    
    TargetInfo() : angle(0), distance(0), detected(false) {}
};

class Server {
private:
    State currentState;
    std::queue<Command> commandQueue;
    std::mutex queueMutex;
    std::atomic<bool> running;
    
    // Детекция QR кодов
    std::vector<TargetInfo> detectQRCodes(const cv::Mat& frame) {
        std::vector<TargetInfo> targets;
        cv::QRCodeDetector qrDecoder;
        
        std::vector<cv::Point> points;
        std::string data = qrDecoder.detectAndDecode(frame, points);
        
        if (!data.empty() && !points.empty()) {
            cv::Point2f center;
            for (const auto& p : points) {
                center += p;
            }
            center /= points.size();
            
            TargetInfo target;
            target.detected = true;
            target.center = center;
            
            if (data.find("robotA") != std::string::npos) {
                target.name = "robotA";
            } else if (data.find("robotB") != std::string::npos) {
                target.name = "robotB";
            } else if (data.find("coffee") != std::string::npos) {
                target.name = "coffee";
            } else {
                target.detected = false;
                return targets;
            }
            
            cv::Point2f frameCenter(frame.cols / 2.0, frame.rows / 2.0);
            double dx = center.x - frameCenter.x;
            double dy = center.y - frameCenter.y;
            target.angle = atan2(dy, dx) * 180.0 / CV_PI;
            
            double width = cv::norm(points[0] - points[1]);
            double height = cv::norm(points[1] - points[2]);
            double avgSize = (width + height) / 2.0;
            target.distance = 1000.0 / avgSize;
            
            targets.push_back(target);
        }
        
        return targets;
    }
    
    // Вычисление команд для движения к цели
    Command calculateCommand(const TargetInfo& robotA, const TargetInfo& coffee) {
        double angleDiff = coffee.angle - robotA.angle;
        
        // Нормализация угла к диапазону [-180, 180]
        while (angleDiff <= -180) angleDiff += 360;
        while (angleDiff > 180) angleDiff -= 360;
        
        Command cmd;
        
        // Определение поворота
        if (angleDiff > 10) {
            cmd.action = "right";
            cmd.time_ms = static_cast<int>(std::abs(angleDiff) * 20); // Пропорциональное время
        } else if (angleDiff < -10) {
            cmd.action = "left";
            cmd.time_ms = static_cast<int>(std::abs(angleDiff) * 20);
        } else {
            // Едем вперед, если угол правильный
            cmd.action = "forward";
            cmd.time_ms = static_cast<int>(coffee.distance * 10);
        }
        
        return cmd;
    }
    
    // Обновление состояния и генерация команд
    void updateState() {
        cv::Mat frame;
        cv::VideoCapture cap(0);
        
        if (!cap.isOpened()) {
            std::cerr << "Ошибка: не удалось открыть камеру" << std::endl;
            return;
        }
        
        while (running && currentState != State::COMPLETED) {
            cap >> frame;
            if (frame.empty()) continue;
            
            auto targets = detectQRCodes(frame);
            
            TargetInfo robotA, coffee;
            for (const auto& t : targets) {
                if (t.name == "robotA") robotA = t;
                if (t.name == "coffee") coffee = t;
            }
            
            if (robotA.detected && coffee.detected) {
                Command cmd = calculateCommand(robotA, coffee);
                
                std::lock_guard<std::mutex> lock(queueMutex);
                commandQueue.push(cmd);
                
                std::cout << "Команда: " << cmd.action << " " << cmd.time_ms << "ms" << std::endl;
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        cap.release();
    }

public:
    Server() : currentState(State::IDLE), running(false) {}
    
    void start() {
        running = true;
        currentState = State::DETECTING;
        
        // Запуск потока обновления состояния
        std::thread updateThread(&Server::updateState, this);
        updateThread.detach();
    }
    
    void stop() {
        running = false;
        currentState = State::IDLE;
        
        std::lock_guard<std::mutex> lock(queueMutex);
        while (!commandQueue.empty()) {
            commandQueue.pop();
        }
    }
    
    // Получить следующую команду для робота
    Command getNextCommand() {
        std::lock_guard<std::mutex> lock(queueMutex);
        
        if (commandQueue.empty()) {
            return Command("stop", 0);
        }
        
        Command cmd = commandQueue.front();
        commandQueue.pop();
        return cmd;
    }
    
    // Проверка наличия команд
    bool hasCommands() {
        std::lock_guard<std::mutex> lock(queueMutex);
        return !commandQueue.empty();
    }
};

// Генерация JSON ответа для робота
std::string generateJSONResponse(const Command& cmd) {
    return "{\n  \"action\": \"" + cmd.action + "\",\n  \"time\": " + 
           std::to_string(cmd.time_ms) + "\n}";
}

int main() {
    Server server;
    server.start();
    
    std::cout << "Сервер запущен. Ожидание команд..." << std::endl;
    
    // Имитация HTTP сервера (для тестирования)
    while (true) {
        if (server.hasCommands()) {
            Command cmd = server.getNextCommand();
            std::string response = generateJSONResponse(cmd);
            std::cout << "Ответ: " << response << std::endl;
            
            // Здесь можно отправить ответ на робота через HTTP
            // Для тестирования просто выводим в консоль
        }
        
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    server.stop();
    return 0;
}