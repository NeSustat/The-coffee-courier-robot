#include <iostream>
#include "httplib.h"
#include "nlohmann/json.hpp" // nlohmann/json.hpp "

using json = nlohmann::json;

int main() {
    httplib::Server svr;
    svr.set_thread_pool_size(1);

    svr.Get("/poll", [&](const httplib::Request& req, httplib::Response& res){
        json command;
        command["action"] = "forward";
        command["time"] = 10;

        res.set_content(command.dump(), "application/json");
    });

    svr.listen("localhost", 8080)
    return 0;
}
