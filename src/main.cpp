#include "logger.hpp"

int main() {
    Logger& logger = Logger::getInstance();
    
    if (!logger.init("logs/server.log", LogLevel::DEBUG)) {
        std::cerr << "Failed to start logger!" << std::endl;
        return 1;
    }

    // Test all five log levels
    logger.debug("This is a DEBUG message");
    logger.info("This is an INFO message");
    logger.warning("This is a WARNING message");
    logger.error("This is an ERROR message");
    logger.critical("This is a CRITICAL message");

    logger.info("Logger test complete!");

    return 0;
}