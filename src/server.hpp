#ifndef SERVER_HPP
#define SERVER_HPP

#include "file_server.hpp"
#include "form_handler.hpp"
#include "socket_wrapper.hpp"

#include <string>
#include <atomic>
#include <memory>

class Server {
public:
    Server(int port, const std::string& webRoot, const std::string& formDataDir);

    // Start the server — blocks until shutdown
    bool start();

    void stop();

    bool isRunning() const { return running_.load(); }

    Server(const Server&) = delete;
    Server& operator=(const Server&) = delete;

private:
    int          port_;
    std::string  webRoot_;
    std::string  formDataDir_;
    std::atomic<bool> running_;    

    std::unique_ptr<FileServer>  fileServer_;   
    std::unique_ptr<FormHandler> formHandler_;   

    SocketWrapper createListeningSocket();

    static void setupSignalHandlers();
    static void signalHandler(int signum);

    static Server* instance_;
};

#endif