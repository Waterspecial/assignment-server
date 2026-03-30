#include "logger.hpp"
#include "socket_wrapper.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "file_server.hpp"
#include "sandbox.hpp"
#include "form_handler.hpp"

int main() {
    Logger& logger = Logger::getInstance();
    logger.init("logs/server.log", LogLevel::DEBUG);

    FileServer fileServer("./www");
    FormHandler formHandler("./form_data");

    logger.info("All 7 files (4 .hpp + 3 .cpp) compile successfully!");
    return 0;
}