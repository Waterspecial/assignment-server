#include "logger.hpp"
#include "socket_wrapper.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "file_server.hpp"

int main() {
    Logger& logger = Logger::getInstance();
    logger.init("logs/server.log", LogLevel::DEBUG);

    // Create a FileServer pointing at our www/ directory
    FileServer fileServer("./www");

    // Test serving the index page
    HttpResponse resp = fileServer.serveFile("/");
    logger.info("GET / → " + std::to_string(resp.build().size()) + " bytes");

    // Test 404
    HttpResponse notFound = fileServer.serveFile("/doesnt_exist.html");
    logger.info("GET /doesnt_exist.html → response built");

    // Test path traversal (should be blocked!)
    HttpResponse traversal = fileServer.serveFile("/../../../etc/passwd");
    logger.info("Path traversal test complete");

    return 0;
}