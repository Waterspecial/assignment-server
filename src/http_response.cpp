#include "http_response.hpp"
#include "logger.hpp"
#include <sys/socket.h>
#include <sstream>

HttpResponse::HttpResponse() : statusCode_(200), reasonPhrase_("OK") {}

void HttpResponse::setStatus(int code, const std::string& reason) {
    statusCode_  = code;
    reasonPhrase_ = reason;
}

void HttpResponse::setHeader(const std::string& key, const std::string& value) {
    headers_[key] = value;
}

void HttpResponse::setBody(const std::string& content) {
    body_ = content;
    // Always set Content-Length to prevent response splitting attacks
    headers_["Content-Length"] = std::to_string(body_.size());
}

void HttpResponse::addSecurityHeaders() {
    // Prevents browser from guessing the content type
    // Without this, a .txt file could be interpreted as HTML (XSS vector)
    headers_["X-Content-Type-Options"] = "nosniff";

    // Prevents our pages from being embedded in frames (clickjacking defence)
    headers_["X-Frame-Options"] = "DENY";

    // Basic XSS protection for older browsers
    headers_["X-XSS-Protection"] = "1; mode=block";

    // Minimal server identification (don't reveal version details)
    headers_["Server"] = "SSS-SecureServer/1.0";

    // Close connection after each request (simplifies state management)
    headers_["Connection"] = "close";
}

std::string HttpResponse::build() const {
    HttpResponse mutable_copy = *this;
    mutable_copy.addSecurityHeaders();

    std::ostringstream response;

    // Status line
    response << "HTTP/1.1 " << mutable_copy.statusCode_ << " "
             << mutable_copy.reasonPhrase_ << "\r\n";

    // Headers
    for (const auto& [key, value] : mutable_copy.headers_) {
        response << key << ": " << value << "\r\n";
    }

    // Blank line separating headers from body
    response << "\r\n";

    // Body
    if (!mutable_copy.body_.empty()) {
        response << mutable_copy.body_;
    }

    return response.str();
}

bool HttpResponse::send(int client_fd) const {
    std::string response = build();
    const char* data = response.c_str();
    size_t remaining = response.size();
    size_t totalSent = 0;

    while (remaining > 0) {
        ssize_t sent = ::send(client_fd, data + totalSent, remaining, 0);
        if (sent <= 0) {
            Logger::getInstance().error("Failed to send response data");
            return false;
        }
        totalSent += static_cast<size_t>(sent);
        remaining -= static_cast<size_t>(sent);
    }

    return true;
}

HttpResponse HttpResponse::ok(const std::string& body, const std::string& contentType) {
    HttpResponse resp;
    resp.setStatus(200, "OK");
    resp.setHeader("Content-Type", contentType);
    resp.setBody(body);
    return resp;
}

HttpResponse HttpResponse::notFound() {
    HttpResponse resp;
    resp.setStatus(404, "Not Found");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    resp.setBody(
        "<!DOCTYPE html><html><head><title>404 Not Found</title></head>"
        "<body><h1>404 - Not Found</h1>"
        "<p>The requested resource could not be found on this server.</p>"
        "</body></html>"
    );
    return resp;
}

HttpResponse HttpResponse::badRequest(const std::string& reason) {
    HttpResponse resp;
    resp.setStatus(400, "Bad Request");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    resp.setBody(
        "<!DOCTYPE html><html><head><title>400 Bad Request</title></head>"
        "<body><h1>400 - Bad Request</h1>"
        "<p>" + reason + "</p></body></html>"
    );
    return resp;
}

HttpResponse HttpResponse::forbidden() {
    HttpResponse resp;
    resp.setStatus(403, "Forbidden");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    resp.setBody(
        "<!DOCTYPE html><html><head><title>403 Forbidden</title></head>"
        "<body><h1>403 - Forbidden</h1>"
        "<p>You do not have permission to access this resource.</p>"
        "</body></html>"
    );
    return resp;
}

HttpResponse HttpResponse::methodNotAllowed() {
    HttpResponse resp;
    resp.setStatus(405, "Method Not Allowed");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    resp.setHeader("Allow", "GET, POST");
    resp.setBody(
        "<!DOCTYPE html><html><head><title>405 Method Not Allowed</title></head>"
        "<body><h1>405 - Method Not Allowed</h1>"
        "<p>This method is not supported. Use GET or POST.</p>"
        "</body></html>"
    );
    return resp;
}

HttpResponse HttpResponse::internalError() {
    HttpResponse resp;
    resp.setStatus(500, "Internal Server Error");
    resp.setHeader("Content-Type", "text/html; charset=utf-8");
    // SECURITY: don't leak internal error details to the client
    resp.setBody(
        "<!DOCTYPE html><html><head><title>500 Internal Server Error</title></head>"
        "<body><h1>500 - Internal Server Error</h1>"
        "<p>An unexpected error occurred. Please try again later.</p>"
        "</body></html>"
    );
    return resp;
}

std::string HttpResponse::getMimeType(const std::string& extension) {
    static const std::unordered_map<std::string, std::string> mimeTypes = {
        {".html", "text/html; charset=utf-8"},
        {".htm",  "text/html; charset=utf-8"},
        {".css",  "text/css; charset=utf-8"},
        {".js",   "application/javascript; charset=utf-8"},
        {".json", "application/json; charset=utf-8"},
        {".png",  "image/png"},
        {".jpg",  "image/jpeg"},
        {".jpeg", "image/jpeg"},
        {".gif",  "image/gif"},
        {".svg",  "image/svg+xml"},
        {".ico",  "image/x-icon"},
        {".txt",  "text/plain; charset=utf-8"},
        {".pdf",  "application/pdf"},
        {".xml",  "application/xml"},
    };

    auto it = mimeTypes.find(extension);
    if (it != mimeTypes.end()) {
        return it->second;
    }

    // Safe default: binary download (browser won't try to execute it)
    return "application/octet-stream";
}