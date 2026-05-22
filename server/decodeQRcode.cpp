#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <cmath>
#include <mutex>
#include "decodeQRcode.h"

struct point{
    int ax;
    int ay;
    int bx;
    int by;
};

namespace QR{

point robot, coffee;
bool running = true;
bool checkAllQR = false;
std::mutex qr_mutex;  // Защита доступа к robot и coffee

bool tryDecode(cv::QRCodeDetector& QR, cv::Mat& processed, cv::Mat& img, std::vector<cv::Point>& points, std::vector<cv::String>& data) {
    if (QR.detectAndDecodeMulti(processed, data, points) && !data.empty())
        return true;
    data.clear();
    points.clear();
    return false;
}

void binarization(cv::Mat& img, cv::Mat& gray){
    cv::adaptiveThreshold(gray, img, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 51, 2);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3,3));
    cv::morphologyEx(img, img, cv::MORPH_CLOSE, kernel);
}

bool check(std::vector<cv::String> data){
    int counter = 0;
    for (int i = 0; i < (int)data.size(); i++){
        std::string s = data[i];
        s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
        if (s == "robotA" || s == "robotB" || s == "coffee")
            counter++;
    }
    if (counter == 3) return true;
    return false;
}

void quality(cv::Mat& img){
    cv::Mat gray;
    if (img.channels() == 3)
        cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
    else
        gray = img.clone();
    cv::GaussianBlur(gray, gray, cv::Size(3,3), 0);
    cv::equalizeHist(gray, gray);
    binarization(img, gray);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3,3));
}

void perspective(std::vector<cv::Point2f>& imgPoint, cv::Mat& img){
    std::vector<cv::Point2f> point = {{0, 0}, {512, 0}, {512, 512}, {0, 512}};
    cv::Mat M = cv::getPerspectiveTransform(imgPoint, point);
    cv::warpPerspective(img, img, M, cv::Size(512, 512), cv::INTER_CUBIC);
}

void findColors(cv::Mat& img){
    cv::Mat hsv;
    cv::cvtColor(img, hsv, cv::COLOR_BGR2HSV);

    auto getCenter = [&](cv::Scalar low, cv::Scalar high) -> cv::Point {
        cv::Mat mask;
        cv::inRange(hsv, low, high, mask);
        std::vector<std::vector<cv::Point>> contours;
        cv::findContours(mask, contours, cv::RETR_EXTERNAL, cv::CHAIN_APPROX_SIMPLE);
        if (contours.empty()) return {0, 0};
        auto& maxC = *std::max_element(contours.begin(), contours.end(),
            [](auto& a, auto& b){ return cv::contourArea(a) < cv::contourArea(b); });
        if (cv::contourArea(maxC) < 500) return {0, 0};
        auto r = cv::boundingRect(maxC);
        return {r.x + r.width/2, r.y + r.height/2};
    };

    auto pink  = getCenter({140, 50, 150}, {170, 255, 255});
    auto green = getCenter({40, 100, 100}, {80, 255, 255});
    auto blue = getCenter({100, 150, 50}, {130, 255, 255});

    if (pink.x)  { robot.bx = pink.x;  robot.by = pink.y; }
    if (green.x) { robot.ax = green.x; robot.ay = green.y; }
    if (blue.x) { coffee.ax = blue.x; coffee.ay = blue.y; }

    checkAllQR = (pink.x && green.x && blue.x);
}

void robotLine(cv::Mat& img){
    cv::Point startPoint = {robot.bx, robot.by};
    cv::Point endPoint = {robot.ax, robot.ay};
    cv::line(img, startPoint, endPoint, cv::Scalar(0, 255, 0), 3);
}

void way(cv::Mat& img) {
    cv::Point startPoint = {(robot.ax + robot.bx) / 2, (robot.ay + robot.by) / 2};
    cv::Point endPoint = {coffee.ax, coffee.ay};
    cv::line(img, startPoint, endPoint, cv::Scalar(0, 255, 0), 3);
    robotLine(img);
}

void decodeQR(cv::Mat& img) {
    cv::QRCodeDetector QR;
    std::vector<cv::Point> points;
    std::vector<cv::String> data;
    cv::Mat gray, processed;

    cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

    if (!tryDecode(QR, gray, img, points, data)) {
        cv::adaptiveThreshold(gray, processed, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 51, 10);
        if (!tryDecode(QR, processed, img, points, data)) {
            cv::resize(gray, processed, cv::Size(), 2.0, 2.0);
            tryDecode(QR, processed, img, points, data);
            for (auto& p : points) { p.x /= 2; p.y /= 2; }
        }
    }

    // Используем mutex при обновлении robot и coffee
    std::lock_guard<std::mutex> lock(qr_mutex);
    
    for (int i = 0; i < (int)data.size(); i++) {
        std::vector<cv::Point> qr_points(points.begin() + i * 4, points.begin() + i * 4 + 4);
        cv::polylines(img, {qr_points}, true, cv::Scalar(0, 255, 0), 3);
        if (!data[i].empty()){
            cv::Point center = {
                (qr_points[0].x + qr_points[1].x + qr_points[2].x + qr_points[3].x) / 4,
                (qr_points[0].y + qr_points[1].y + qr_points[2].y + qr_points[3].y) / 4
            };
            std::string s = data[i];
            s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
            if (s == "robotA") {
                robot.ax = center.x;
                robot.ay = center.y;
            } else if (s == "robotB") {
                robot.bx = center.x;
                robot.by = center.y;
            } else if (s == "coffee") {
                coffee.ax = center.x;
                coffee.ay = center.y;
            }
        }
    }
    checkAllQR = (robot.ax != 0 && robot.bx != 0 && coffee.ax != 0);
}

// ВАЖНО: Функции getAngle() и getWay() теперь защищены mutex
double getAngle() {
    std::lock_guard<std::mutex> lock(qr_mutex);
    
    // направление "перед" робота: от B к A
    double robotDirX = robot.ax - robot.bx;
    double robotDirY = robot.ay - robot.by;

    // центр робота
    double centerX = (robot.ax + robot.bx) / 2.0;
    double centerY = (robot.ay + robot.by) / 2.0;

    // направление к кофе
    double toCoffeeX = coffee.ax - centerX;
    double toCoffeeY = coffee.ay - centerY;

    // угол между векторами
    double dot = robotDirX * toCoffeeX + robotDirY * toCoffeeY;
    double cross = robotDirX * toCoffeeY - robotDirY * toCoffeeX;
    double angle = atan2(cross, dot) * 180.0 / CV_PI;
    
    return angle;
}

double getWay(){
    std::lock_guard<std::mutex> lock(qr_mutex);
    
    // центр робота
    double centerX = (robot.ax + robot.bx) / 2.0;
    double centerY = (robot.ay + robot.by) / 2.0;

    // направление к кофе
    double toCoffeeX = coffee.ax - centerX;
    double toCoffeeY = coffee.ay - centerY;

    return sqrt(toCoffeeX * toCoffeeX + toCoffeeY * toCoffeeY);
}

void close(){
    running = false;
}

void run(){
    cv::VideoCapture cap(0);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    if (!cap.isOpened()) return;

    cv::Mat frame;
    int frame_count = 0;
    
    while (running) {
        cap >> frame;
        cv::imshow("frame", frame);
        decodeQR(frame);
        way(frame);
        
        // Выводим статус каждый 30й кадр (примерно раз в секунду при 30fps)
        if (frame_count % 30 == 0) {
            std::cout << "Distance: " << QR::getWay() << " | Angle: " << QR::getAngle() 
                      << " | QR detected: " << (QR::checkAllQR ? "YES" : "NO") << std::endl;
        }
        
        frame_count++;
        if (cv::waitKey(1) == 'q') break;
    }
    cap.release();
    cv::destroyAllWindows();
    return;
}

}  // namespace QR