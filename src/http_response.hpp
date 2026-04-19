#ifndef HTTP_RESPONSE_HPP
#define HTTP_RESPONSE_HPP

#include <string>
#include <unordered_map>

class HttpResponse {
public:
    HttpResponse();

    void setStatus(int code, const std::string& reason);

    void setHeader(const std::string& key, const std::string& value);

    void setBody(const std::string& content);

    std::string build() const;

    bool send(int client_fd) const;

    static HttpResponse ok(const std::string& body, const std::string& contentType);
    static HttpResponse notFound();
    static HttpResponse badRequest(const std::string& reason = "Bad Request");
    static HttpResponse forbidden();
    static HttpResponse methodNotAllowed();
    static HttpResponse internalError();

    static std::string getMimeType(const std::string& extension);

private:
    int         statusCode_;
    std::string reasonPhrase_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;

    void addSecurityHeaders();
};

#endif