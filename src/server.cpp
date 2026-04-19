#include "server.hpp"
#include "connection_handler.hpp"
#include "logger.hpp"

#include <sys/socket.h>
#include <netinet/in.h>  // sockaddr_in
#include <arpa/inet.h>   // inet_ntop
#include <unistd.h>
#include <csignal>
#include <cstring>
#include <thread>
#include <sys/wait.h>

// Initialise the static instance pointer
Server* Server::instance_ = nullptr;
Server::Server(int port, const std::string& webRoot, const std::string& formDataDir)
    : port_(port)
    , webRoot_(webRoot)
    , formDataDir_(formDataDir)
    , running_(false)
{
    instance_ = this;
}

bool Server::start() {
    Logger& logger = Logger::getInstance();
    
    setupSignalHandlers();

    fileServer_  = std::make_unique<FileServer>(webRoot_);
    formHandler_ = std::make_unique<FormHandler>(formDataDir_);

    SocketWrapper listenSocket = createListeningSocket();
    if (!listenSocket.isValid()) {
        logger.critical("Failed to create listening socket — aborting");
        return false;
    }

    running_ = true;
    logger.info("=== SSS Secure Web Server started on port "
                + std::to_string(port_) + " ===");
    logger.info("Serving files from: " + webRoot_);
    logger.info("Form data stored in: " + formDataDir_);
    logger.info("Press Ctrl+C to stop the server.");

    // Step 4: Accept loop — runs until Ctrl+C
    while (running_.load()) {
        // Prepare to receive client address info
        struct sockaddr_in clientAddr;
        socklen_t clientLen = sizeof(clientAddr);
        std::memset(&clientAddr, 0, sizeof(clientAddr));

        // Wait for a client to connect
        int clientFd = accept(listenSocket.get(),
                              reinterpret_cast<struct sockaddr*>(&clientAddr),
                              &clientLen);

        // Check if we're shutting down
        if (!running_.load()) {
            if (clientFd >= 0) close(clientFd);
            break;
        }

        if (clientFd < 0) {
            if (errno == EINTR) {
                continue;  // Interrupted by signal, try again
            }
            logger.error("accept() failed: " + std::string(strerror(errno)));
            continue;  // Don't crash the server for one failed accept
        }

        // Format client address for logging
        char addrStr[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &clientAddr.sin_addr, addrStr, sizeof(addrStr));
        std::string clientAddrStr = std::string(addrStr) + ":"
                                  + std::to_string(ntohs(clientAddr.sin_port));

        logger.info("Accepted connection from: " + clientAddrStr);

        // Wrap client socket in RAII
        SocketWrapper clientSocket(clientFd);

        // Step 5: Spawn a handler thread
        try {
            std::thread handlerThread([this, clientAddrStr](SocketWrapper sock) {
                ConnectionHandler handler(
                    std::move(sock),
                    *fileServer_,
                    *formHandler_,
                    clientAddrStr
                );
                handler.handle();
            }, std::move(clientSocket));

            // Detach — thread runs independently and cleans up after itself
            handlerThread.detach();

        } catch (const std::system_error& e) {
            logger.error("Failed to create thread: " + std::string(e.what()));
        }
    }

    logger.info("=== SSS Secure Web Server stopped ===");
    return true;
}

void Server::stop() {
    running_ = false;
    Logger::getInstance().info("Shutdown requested");
}

SocketWrapper Server::createListeningSocket() {
    Logger& logger = Logger::getInstance();

    // Create a TCP socket
    int sockFd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockFd < 0) {
        logger.critical("Failed to create socket: " + std::string(strerror(errno)));
        return SocketWrapper(-1);
    }

    // Wrap in RAII immediately
    SocketWrapper listenSocket(sockFd);

    // Allow quick restart (skip TIME_WAIT)
    int optVal = 1;
    if (setsockopt(listenSocket.get(), SOL_SOCKET, SO_REUSEADDR,
                   &optVal, sizeof(optVal)) < 0) {
        logger.warning("Failed to set SO_REUSEADDR: " + std::string(strerror(errno)));
    }

    // Configure address: listen on all interfaces, on our port
    struct sockaddr_in serverAddr;
    std::memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(static_cast<uint16_t>(port_));

    // Bind to the address
    if (bind(listenSocket.get(), reinterpret_cast<struct sockaddr*>(&serverAddr),
             sizeof(serverAddr)) < 0) {
        logger.critical("Failed to bind to port " + std::to_string(port_)
                       + ": " + std::string(strerror(errno)));
        return SocketWrapper(-1);
    }

    // Start listening (queue up to 128 pending connections)
    if (listen(listenSocket.get(), 128) < 0) {
        logger.critical("Failed to listen: " + std::string(strerror(errno)));
        return SocketWrapper(-1);
    }

    logger.info("Listening on port " + std::to_string(port_));
    return listenSocket;
}

void Server::setupSignalHandlers() {
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));

    // Ctrl+C and kill command → graceful shutdown
    sa.sa_handler = signalHandler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGINT,  &sa, nullptr);
    sigaction(SIGTERM, &sa, nullptr);

    // Ignore SIGPIPE — prevents crash when client disconnects mid-response
    sa.sa_handler = SIG_IGN;
    sigaction(SIGPIPE, &sa, nullptr);

    Logger::getInstance().debug("Signal handlers installed");
}

void Server::signalHandler(int signum) {
    const char* msg = "\n[SIGNAL] Shutdown signal received\n";
    ssize_t ret = write(STDOUT_FILENO, msg, strlen(msg));
    (void)ret;

    if (instance_ != nullptr) {
        instance_->stop();
    }

    // Second signal = force quit
    struct sigaction sa;
    std::memset(&sa, 0, sizeof(sa));
    sa.sa_handler = SIG_DFL;
    sigaction(signum, &sa, nullptr);
}