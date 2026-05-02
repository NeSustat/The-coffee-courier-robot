cmake_minimum_required(VERSION 3.15)
project(Robot LANGUAGES CXX)

add_compile_definitions(_WIN32_WINNT=0x0B00)  # Windows 11


include(FetchContent)

# Declare the external library
FetchContent_Declare(
  httplib
  GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
  GIT_TAG        v0.12.6
  GIT_SHALLOW    TRUE
)
#   GIT_TAG        v0.40 # Use a specific tag, commit hash, or branch


FetchContent_MakeAvailable(httplib)


FetchContent_Declare(
  json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
)
FetchContent_MakeAvailable(json)

#   GIT_TAG v3.12.0 # Use a specific version tag

add_executable(myexample main.cpp)

target_link_libraries(myexample PRIVATE httplib)
target_link_libraries(myexample PRIVATE nlohmann_json::nlohmann_json)