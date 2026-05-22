#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>

struct TargetInfo {
    std::string name;
    double angle;
    double distance;
    bool detected;
};

// Детекция QR кодов и извлечение данных
std::vector<TargetInfo> detectQRCodes(const cv::Mat& frame) {
    std::vector<TargetInfo> targets;
    cv::QRCodeDetector qrDecoder;
    
    std::vector<cv::Point> points;
    std::string data = qrDecoder.detectAndDecode(frame, points);
    
    if (!data.empty() && !points.empty()) {
        // Вычисляем центр QR кода
        cv::Point2f center;
        for (const auto& p : points) {
            center += p;
        }
        center /= points.size();
        
        // Определяем тип цели по содержимому
        TargetInfo target;
        target.detected = true;
        
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
        
        // Вычисляем угол относительно центра кадра
        cv::Point2f frameCenter(frame.cols / 2.0, frame.rows / 2.0);
        double dx = center.x - frameCenter.x;
        double dy = center.y - frameCenter.y;
        target.angle = atan2(dy, dx) * 180.0 / CV_PI;
        
        // Оценка дистанции по размеру QR кода (примерная)
        double width = cv::norm(points[0] - points[1]);
        double height = cv::norm(points[1] - points[2]);
        double avgSize = (width + height) / 2.0;
        // Примерная дистанция: чем меньше QR код, тем дальше
        target.distance = 1000.0 / avgSize; // Условные единицы
        
        targets.push_back(target);
    }
    
    return targets;
}

// Получение информации о роботе A и кофе
TargetInfo getRobotAToCoffeeInfo(const std::vector<TargetInfo>& targets) {
    TargetInfo robotA, coffee;
    bool foundRobotA = false, foundCoffee = false;
    
    for (const auto& t : targets) {
        if (t.name == "robotA") {
            robotA = t;
            foundRobotA = true;
        } else if (t.name == "coffee") {
            coffee = t;
            foundCoffee = true;
        }
    }
    
    if (foundRobotA && foundCoffee) {
        // Угол от робота A к кофе
        TargetInfo result;
        result.name = "robotA_to_coffee";
        result.angle = coffee.angle - robotA.angle;
        result.distance = cv::norm(cv::Point2f(robotA.angle, robotA.distance) - 
                                   cv::Point2f(coffee.angle, coffee.distance));
        result.detected = true;
        return result;
    }
    
    TargetInfo empty;
    empty.detected = false;
    return empty;
}

int main() {
    cv::VideoCapture cap(0); // Открыть камеру
    
    if (!cap.isOpened()) {
        std::cerr << "Ошибка: не удалось открыть камеру" << std::endl;
        return -1;
    }
    
    cv::Mat frame;
    
    while (true) {
        cap >> frame;
        if (frame.empty()) break;
        
        auto targets = detectQRCodes(frame);
        
        // Показать результаты
        for (const auto& t : targets) {
            if (t.detected) {
                std::cout << "Обнаружен: " << t.name 
                         << ", угол: " << t.angle 
                         << ", дистанция: " << t.distance << std::endl;
            }
        }
        
        // Получить информацию о пути от робота A к кофе
        auto pathInfo = getRobotAToCoffeeInfo(targets);
        if (pathInfo.detected) {
            std::cout << "Путь robotA -> coffee: угол=" << pathInfo.angle 
                     << ", дистанция=" << pathInfo.distance << std::endl;
        }
        
        // Показать кадр
        cv::imshow("QR Detection", frame);
        if (cv::waitKey(1) == 27) break; // ESC для выхода
    }
    
    cap.release();
    cv::destroyAllWindows();
    return 0;
}