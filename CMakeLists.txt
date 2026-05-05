cmake_minimum_required(VERSION 3.15)
project(robot_Nasil LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)

# OpenCV
set(OpenCV_DIR "${CMAKE_SOURCE_DIR}/opencv/x64/mingw/lib")
find_package(OpenCV REQUIRED)

# httplib
include(FetchContent)
FetchContent_Declare(
    httplib
    GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
    GIT_TAG        v0.12.6
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(httplib)

# nlohmann/json
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(json)

# decodeQRcode
add_executable(decodeQRcode server/decodeQRcode.cpp)
target_link_libraries(decodeQRcode PRIVATE ${OpenCV_LIBS})

# server
add_executable(server server/server.cpp)
target_link_libraries(server PRIVATE httplib nlohmann_json::nlohmann_json)

# main (robot client)
add_executable(robot client/robot.cpp)
target_link_libraries(robot PRIVATE httplib nlohmann_json::nlohmann_json)
