#pragma once
#include <opencv2/opencv.hpp>
#include <vector>
#include <iostream>
#include <cmath>

struct point{
    int ax;
    int ay;
    int bx;
    int by;
};

point robot, coffee;

// bool tryDecode(cv::QRCodeDetector& QR, cv::Mat& processed, cv::Mat& img, std::vector<cv::Point>& points, std::vector<cv::String>& data) {
//     if (QR.detectAndDecodeMulti(processed, data, points) && !data.empty())
//         return true;
//     data.clear();
//     points.clear();
//     return false;
// }

void binarization(cv::Mat& img, cv::Mat& gray){
    cv::adaptiveThreshold(gray, img, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 51, 2);
    cv::Mat kernel = cv::getStructuringElement(cv::MORPH_RECT, cv::Size(3,3));
    cv::morphologyEx(img, img, cv::MORPH_CLOSE, kernel);
}

// void binarization(cv::Mat& img){
//     cv::Mat gray;
//     if (img.channels() == 3)
//         cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);
//     else
//         gray = img.clone();
//     cv::equalizeHist(gray, gray);
//     cv::threshold(gray, img, 50, 205, cv::THRESH_BINARY | cv::THRESH_OTSU);
// }

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
    // cv::morphologyEx(img, img, cv::MORPH_OPEN, kernel);
    // cv::morphologyEx(img, img, cv::MORPH_CLOSE, kernel);
}

void perspective(std::vector<cv::Point2f>& imgPoint, cv::Mat& img){
    std::vector<cv::Point2f> point = {{0, 0}, {512, 0}, {512, 512}, {0, 512}};
    cv::Mat M = cv::getPerspectiveTransform(imgPoint, point);

    cv::warpPerspective(img, img, M, cv::Size(512, 512), cv::INTER_CUBIC);
}

void decodeQR(cv::Mat& img){
    cv::QRCodeDetector QR;
    quality(img);
    std::vector<cv::Point2f> point;
    std::vector<cv::String> data;
    if (QR.detectMulti(img, point) && !point.empty()){
        for (int i = 0; i < (int)point.size() / 4; i++){
            // находим центр
            cv::Point2f center(0, 0);
            for (int j = i*4; j < i*4+4; j++)
                center += point[j];
            center *= 0.25f;

            // расширяем точки от центра
            std::vector<cv::Point2f> qrPoints;
            for (int j = i*4; j < i*4+4; j++){
                cv::Point2f dir = point[j] - center;
                qrPoints.push_back(point[j] + dir * 0.1f); // 0.1f = 10% отступ
            }

            // std::vector<cv::Point2f> qrPoints = {
            //     point[i * 4],
            //     point[i * 4 + 1],
            //     point[i * 4 + 2],
            //     point[i * 4 + 3]
            // };

            perspective(qrPoints, img);

            QR.detectAndDecodeMulti(img, data, point);
            
            for (int i = 0; i < (int)data.size(); i++){
                std::cout << "detect QR Code: " << (int)data.size() << std::endl;
                std::cout << "[" << data[i] << "]" << std::endl;
            }
        }
    }
    point.clear();
    data.clear();
}

void robotLine(cv::Mat& img){
    cv::Point startPoint = {robot.bx, robot.by};
    cv::Point endPoint = {robot.ax, robot.ay};
    // std::cout << "start: " << startPoint << " end: " << endPoint << std::endl;
    cv::line(img, startPoint, endPoint, cv::Scalar(0, 255, 0), 3);
}

void way(cv::Mat& img) {
    cv::Point startPoint = {(robot.ax + robot.bx) / 2, (robot.ay + robot.by) / 2};
    cv::Point endPoint = {coffee.ax, coffee.ay};
    // std::cout << "start: " << startPoint << " end: " << endPoint << std::endl;
    cv::line(img, startPoint, endPoint, cv::Scalar(0, 255, 0), 3);
    robotLine(img);
}

// void decodeQR(cv::Mat& img) {
//     cv::QRCodeDetector QR;
//     std::vector<cv::Point> points;
//     std::vector<cv::String> data;
//     cv::Mat gray, processed;

//     cv::cvtColor(img, gray, cv::COLOR_BGR2GRAY);

//     if (!tryDecode(QR, gray, img, points, data)) {
//         cv::adaptiveThreshold(gray, processed, 255, cv::ADAPTIVE_THRESH_GAUSSIAN_C, cv::THRESH_BINARY, 51, 10);
//         if (!tryDecode(QR, processed, img, points, data)) {
//             cv::resize(gray, processed, cv::Size(), 2.0, 2.0);
//             tryDecode(QR, processed, img, points, data);
//             for (auto& p : points) { p.x /= 2; p.y /= 2; }
//         }
//     }

//     for (int i = 0; i < (int)data.size(); i++) {
//         std::vector<cv::Point> qr_points(points.begin() + i * 4, points.begin() + i * 4 + 4);
//         cv::polylines(img, {qr_points}, true, cv::Scalar(0, 255, 0), 3);
//         if (!data[i].empty()){
//             cv::Point center = {
//                 (qr_points[0].x + qr_points[1].x + qr_points[2].x + qr_points[3].x) / 4,
//                 (qr_points[0].y + qr_points[1].y + qr_points[2].y + qr_points[3].y) / 4
//             };
//             std::string s = data[i];
//             s.erase(remove_if(s.begin(), s.end(), ::isspace), s.end());
//             // std::cout << data[i] << std::endl;
//             if (s == "robotA") {
//                 robot.ax = center.x;
//                 robot.ay = center.y;
//             } else if (s == "robotB") {
//                 robot.bx = center.x;
//                 robot.by = center.y;
//             } else if (s == "coffee") {
//                 coffee.ax = center.x;
//                 coffee.ay = center.y;
//             }
//             // std::cout << qr_points[i] << std::endl;
//         }
//     }
// }

double getAngle() {
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
        // центр робота
    double centerX = (robot.ax + robot.bx) / 2.0;
    double centerY = (robot.ay + robot.by) / 2.0;

    // направление к кофе
    double toCoffeeX = coffee.ax - centerX;
    double toCoffeeY = coffee.ay - centerY;

    return sqrt(toCoffeeX * toCoffeeX + toCoffeeY * toCoffeeY);
}

int main() {
    cv::VideoCapture cap(0);
    cap.set(cv::CAP_PROP_FRAME_WIDTH, 1280);
    cap.set(cv::CAP_PROP_FRAME_HEIGHT, 720);
    if (!cap.isOpened()) return -1;

    cv::Mat frame;
    while (true) {
        cap >> frame;
        decodeQR(frame);
        // way(frame);
        // cv::imshow("frame", frame);
        // std::cout << getAngle() << std::endl;
        // if (cv::waitKey(1) == 'q') break;
        // if (cv::getWindowProperty("frame", cv::WND_PROP_VISIBLE) < 1) break;
    }
    cap.release();
    cv::destroyAllWindows();
    return 0;
}