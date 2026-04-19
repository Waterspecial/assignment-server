#include "server.hpp"
#include "logger.hpp"

#include <iostream>
#include <string>
#include <sys/stat.h>
#include <cstdlib>

// Create a directory if it doesn't exist
static bool ensureDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) {
        if (S_ISDIR(st.st_mode)) {
            return true;  // Already exists
        }
        return false;  // Exists but not a directory
    }
    // Create with permissions: owner=rwx, group=r-x, others=none
    return mkdir(path.c_str(), 0750) == 0;
}

// Validate port number range
static bool isValidPort(int port) {
    return port > 0 && port <= 65535;
}

int main(int argc, char* argv[]) {
    // Check for help flag
    if (argc > 1 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help")) {
        std::cout << "SSS Secure Web Server\n"
                  << "Usage: " << argv[0] << " [port] [web_root] [form_data_dir]\n"
                  << "Defaults: port=8080, web_root=./www, form_data_dir=./form_data\n";
        return 0;
    }

    // Parse arguments with defaults
    int port = 8080;
    std::string webRoot     = "./www";
    std::string formDataDir = "./form_data";
    std::string logDir      = "./logs";

    if (argc >= 2) {
        port = std::atoi(argv[1]);
        if (!isValidPort(port)) {
            std::cerr << "Error: Invalid port '" << argv[1] << "'. Must be 1-65535.\n";
            return 1;
        }
    }
    if (argc >= 3) {
        webRoot = argv[2];
    }
    if (argc >= 4) {
        formDataDir = argv[3];
    }

    // Create directories
    if (!ensureDirectory(logDir)) {
        std::cerr << "Error: Failed to create log directory\n";
        return 1;
    }
    if (!ensureDirectory(formDataDir)) {
        std::cerr << "Error: Failed to create form data directory\n";
        return 1;
    }
    if (!ensureDirectory(webRoot)) {
        std::cerr << "Error: Web root doesn't exist: " << webRoot << "\n";
        return 1;
    }

    // Start logger first (everything else needs it)
    Logger& logger = Logger::getInstance();
    if (!logger.init(logDir + "/server.log", LogLevel::DEBUG)) {
        std::cerr << "Error: Failed to start logger\n";
        return 1;
    }

    logger.info("========================================");
    logger.info("SSS Secure Web Server starting up");
    logger.info("Port:          " + std::to_string(port));
    logger.info("Web root:      " + webRoot);
    logger.info("Form data dir: " + formDataDir);
    logger.info("========================================");

    // Create and start the server
    Server server(port, webRoot, formDataDir);

    if (!server.start()) {
        logger.critical("Server failed to start");
        return 1;
    }

    logger.info("Server exited cleanly");
    return 0;
}