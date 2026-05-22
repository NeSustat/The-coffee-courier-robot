#include "decodeQRcode.h"
#include <opencv2/opencv.hpp>
#include <opencv2/objdetect.hpp>
#include <cmath>

// Детекция всех QR кодов на кадре
std::vector<QRTarget> detectQRCodes(const cv::Mat& frame) {
    std::vector<QRTarget> targets;
    cv::QRCodeDetector qrDecoder;
    
    std::vector<std::string> decodedInfo;
    std::vector<cv::Point> points;
    std::vector<cv::Mat> straightQrCodes;
    
    bool detected = qrDecoder.detectAndDecodeMulti(frame, decodedInfo, points, straightQrCodes);
    
    if (detected && !decodedInfo.empty()) {
        size_t numQR = decodedInfo.size();
        
        for (size_t i = 0; i < numQR; i++) {
            std::string data = decodedInfo[i];
            
            // Определяем тип QR кода
            QRTarget target;
            target.detected = false;
            
            if (data.find("robotA") != std::string::npos) {
                target.name = "robotA";
                target.detected = true;
            } else if (data.find("robotB") != std::string::npos) {
                target.name = "robotB";
                target.detected = true;
            } else if (data.find("coffee") != std::string::npos) {
                target.name = "coffee";
                target.detected = true;
            }
            
            if (target.detected) {
                // Вычисляем центр QR кода (4 точки на каждый QR)
                size_t startIdx = i * 4;
                cv::Point2f center(0, 0);
                for (size_t j = 0; j < 4; j++) {
                    center.x += points[startIdx + j].x;
                    center.y += points[startIdx + j].y;
                }
                center.x /= 4.0;
                center.y /= 4.0;
                target.center = center;
                
                // Угол относительно центра кадра (горизонтальное отклонение)
                cv::Point2f frameCenter(frame.cols / 2.0, frame.rows / 2.0);
                double dx = center.x - frameCenter.x;
                // Положительный угол = справа, отрицательный = слева
                target.angle = (dx / frameCenter.x) * 45.0; // Примерно ±45° на края кадра
                
                // Дистанция по размеру QR кода
                double width = cv::norm(points[startIdx] - points[startIdx + 1]);
                double height = cv::norm(points[startIdx + 1] - points[startIdx + 2]);
                double avgSize = (width + height) / 2.0;
                target.distance = 1000.0 / avgSize; // Условные единицы
                
                targets.push_back(target);
            }
        }
    }
    
    return targets;
}

// Расчет угла поворота и дистанции от robotA до coffee
NavigationData calculateNavigation(const std::vector<QRTarget>& targets) {
    NavigationData nav;
    nav.valid = false;
    
    QRTarget robotA, coffee;
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
        // Угол поворота: разница углов между coffee и robotA
        // Положительный = поворот вправо, отрицательный = влево
        nav.turnAngle = coffee.angle - robotA.angle;
        
        // Нормализация угла к диапазону [-180, 180]
        while (nav.turnAngle > 180) nav.turnAngle -= 360;
        while (nav.turnAngle < -180) nav.turnAngle += 360;
        
        // Дистанция между robotA и coffee
        double dx = coffee.center.x - robotA.center.x;
        double dy = coffee.center.y - robotA.center.y;
        nav.distanceToCoffee = std::sqrt(dx * dx + dy * dy);
        
        nav.valid = true;
    }
    
    return nav;
}
