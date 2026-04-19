#include "connection_handler.hpp"
#include "http_request.hpp"
#include "http_response.hpp"
#include "logger.hpp"

ConnectionHandler::ConnectionHandler(SocketWrapper clientSocket,
                                     FileServer& fileServer,
                                     FormHandler& formHandler,
                                     const std::string& clientAddr)
    : clientSocket_(std::move(clientSocket))  
    , fileServer_(fileServer)
    , formHandler_(formHandler)
    , clientAddr_(clientAddr)
{
}

void ConnectionHandler::handle() {
    Logger& logger = Logger::getInstance();
    logger.info("Handling connection from: " + clientAddr_);

    try {
        // Step 1: Parse the HTTP request
        auto requestOpt = HttpRequest::parse(clientSocket_.get());

        if (!requestOpt.has_value()) {
            logger.warning("Failed to parse request from: " + clientAddr_);
            HttpResponse::badRequest("Malformed HTTP request.").send(clientSocket_.get());
            return;
        }

        const HttpRequest& request = requestOpt.value();

        // Step 2: Route to the right handler
        HttpResponse response;

        switch (request.getMethod()) {
            case HttpMethod::GET: {
                logger.debug("Routing GET to FileServer: " + request.getPath());

                // Check if this is a GET form submission (has query string)
                if (!request.getQueryString().empty() &&
                    request.getPath() == "/submit") {
                    logger.info("GET form submission from: " + clientAddr_);
                    response = formHandler_.handleForm(request, clientSocket_.get());
                } else {
                    response = fileServer_.serveFile(request.getPath());
                }
                break;
            }

            case HttpMethod::POST: {
                logger.info("POST from: " + clientAddr_ + " to: " + request.getPath());
                response = formHandler_.handleForm(request, clientSocket_.get());
                break;
            }

            case HttpMethod::UNSUPPORTED: {
                logger.warning("Unsupported method '" + request.getMethodString()
                              + "' from: " + clientAddr_);
                response = HttpResponse::methodNotAllowed();
                break;
            }
        }

        // Step 3: Send the response
        if (!response.send(clientSocket_.get())) {
            logger.error("Failed to send response to: " + clientAddr_);
        } else {
            logger.info("Response sent to: " + clientAddr_);
        }

    } catch (const std::exception& e) {
       
        logger.error("Exception from " + clientAddr_ + ": " + e.what());
        try {
            HttpResponse::internalError().send(clientSocket_.get());
        } catch (...) {
            logger.error("Failed to send error response to: " + clientAddr_);
        }
    } catch (...) {
        logger.critical("Unknown exception from: " + clientAddr_);
        try {
            HttpResponse::internalError().send(clientSocket_.get());
        } catch (...) {}
    }

    logger.debug("Connection closed for: " + clientAddr_);
}