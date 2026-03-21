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

bool tryDecode(cv::QRCodeDetector& QR, cv::Mat& processed, cv::Mat& img, std::vector<cv::Point>& points, std::vector<cv::String>& data) {
    if (QR.detectAndDecodeMulti(processed, data, points) && !data.empty())
        return true;
    data.clear();
    points.clear();
    return false;
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
            // std::cout << data[i] << std::endl;
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
            // std::cout << qr_points[i] << std::endl;
        }
    }
}

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

int main() {
    cv::VideoCapture cap(0);
    if (!cap.isOpened()) return -1;

    cv::Mat frame;
    while (true) {
        cap >> frame;
        decodeQR(frame);
        way(frame);
        cv::imshow("frame", frame);
        std::cout << getAngle() << std::endl;
        if (cv::waitKey(1) == 'q') break;
        if (cv::getWindowProperty("frame", cv::WND_PROP_VISIBLE) < 1) break;
    }
    cap.release();
    cv::destroyAllWindows();
    return 0;
}
