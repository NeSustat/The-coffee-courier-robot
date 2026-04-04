#include <iostream>
#include "httplib.h"
#include "nlohmann/json.hpp" // nlohmann/json.hpp "
using json = nlohmann::json;
int main() {
    httplib::Server svr;
    svr.Post("/commands", [](const httplib::Request& req, httplib::Response& res) {
        std::string request_body = req.body;
        if (req.get_header_value("Content-Type") != "application/json") {
            res.status = 415; // Unsupported Media Type
            res.set_content("Unsupported Content-Type. Expected application/json", "text/plain");
            return;
        }
        try {
            json request_json = json::parse(request_body);
            // Access data from the JSON object
            std::string name = request_json.value("command", "none");
            int id = request_json.value("id", 0);
            // Create a JSON response object
            json response_json;
            response_json["message"] = "Command received successfully";
            response_json["received_id"] = id;
            response_json["status"] = "success";
            res.set_content(response_json.dump(), "application/json");
            res.status = 200; // OK
        } catch (const json::parse_error& e) {
            res.status = 400; // Bad Request
            res.set_content("Invalid JSON format: " + std::string(e.what()), "text/plain");
        }
    });
    std::cout << "Server listening on http://localhost:8080" << std::endl;
    std::cout << "Try: curl -X POST -H \"Content-Type: application/json\" -d '{\"command\": \"forward\", \"time\": 10, \"id\": 123}' http://localhost:8080/data" << std::endl;    
    svr.listen("0.0.0.0", 8080);
    return 0;
}
