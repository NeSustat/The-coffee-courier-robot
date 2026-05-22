#ifndef DECODE_QRCODE_H
#define DECODE_QRCODE_H

#include <opencv2/opencv.hpp>
#include <string>
#include <vector>

// Структура для хранения информации о QR коде
struct QRTarget {
    std::string name;
    cv::Point2f center;
    double angle;
    double distance;
    bool detected;
};

// Структура для навигационных данных
struct NavigationData {
    double turnAngle;        // Угол поворота: + вправо, - влево
    double distanceToCoffee; // Дистанция до кофе
    bool valid;
};

// Детекция всех QR кодов на кадре
std::vector<QRTarget> detectQRCodes(const cv::Mat& frame);

// Расчет угла поворота и дистанции от robotA до coffee
NavigationData calculateNavigation(const std::vector<QRTarget>& targets);

#endif
