#ifndef CONNECTION_HANDLER_HPP
#define CONNECTION_HANDLER_HPP

#include "file_server.hpp"
#include "form_handler.hpp"
#include "socket_wrapper.hpp"

#include <string>

class ConnectionHandler {
public:

    ConnectionHandler(SocketWrapper clientSocket,
                      FileServer& fileServer,
                      FormHandler& formHandler,
                      const std::string& clientAddr);

    void handle();

private:
    SocketWrapper clientSocket_;  
    FileServer&   fileServer_;    
    FormHandler&  formHandler_;   
    std::string   clientAddr_;    
};

#endif