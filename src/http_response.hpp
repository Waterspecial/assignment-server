#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <unordered_map>

class HttpResponse {
public:
    HttpResponse();

    // Set the status code and reason (e.g. 200, "OK")
    void setStatus(int code, const std::string& reason);

    // Set a response header
    void setHeader(const std::string& key, const std::string& value);

    // Set the body (automatically sets Content-Length)
    void setBody(const std::string& content);

    // Convert everything into a raw HTTP string
    std::string build() const;

    // Send the response over a socket (handles partial writes)
    bool send(int client_fd) const;

    // Quick factory methods for common responses
    static HttpResponse ok(const std::string& body, const std::string& contentType);
    static HttpResponse notFound();
    static HttpResponse badRequest(const std::string& reason = "Bad Request");
    static HttpResponse forbidden();
    static HttpResponse methodNotAllowed();
    static HttpResponse internalError();

    // Map file extension to MIME type (e.g. ".html" → "text/html")
    static std::string getMimeType(const std::string& extension);

private:
    int         statusCode_;
    std::string reasonPhrase_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;

    // Adds security headers (called automatically during build)
    void addSecurityHeaders();
};

#endif // HTTP_RESPONSE_HPP