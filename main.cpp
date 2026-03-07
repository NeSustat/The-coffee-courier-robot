#include <iostream>
#include <cmath>
#include <thread>

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
        std::cout << "robot turns to the right\n\n";
        thr(time_ms);
    }
    
    void right(int time_ms){
        std::cout << "robot turns to the left\n\n";
        thr(time_ms);
    }

    void stop(){
        std::cout << "power off\n\n";
        
    }
};

int main(){
    FooEngine robot;
    int chose = 0;
    int time = 0;
    while(1){
        std::cout << "1-move\n2-rotate left\n3-rotate right\n4-stop\n";
        std::cin >> chose;
        switch (chose){
        case 1:
            std::cout << "write time move\n";
            std::cin >> time;
            robot.forward(time);
            chose = 0;
            break;
        case 2:
            std::cout << "write time rotate\n";
            std::cin >> time;
            robot.left(time);
            chose = 0;
            break;
        case 3:
            std::cout << "write time rotate\n";
            std::cin >> time;
            robot.left(time);
            chose = 0;
            break;
        case 4:
            robot.stop();
            chose = 0;
            break;
        case 5:
            return 0;
        default:
            std::cout << "error\n";
            break;
        }
        time = 0;
    }
}