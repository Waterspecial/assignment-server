#ifndef HTTP_REQUEST_HPP
#define HTTP_REQUEST_HPP

#include <optional>
#include <string>
#include <unordered_map>

enum class HttpMethod {
    GET,
    POST,
    UNSUPPORTED
};

class HttpRequest {
public:
    // Size limits to prevent attacks
    static constexpr size_t MAX_REQUEST_LINE_LENGTH = 8192;   
    static constexpr size_t MAX_HEADER_COUNT        = 100;    
    static constexpr size_t MAX_BODY_SIZE           = 1048576; 
    static constexpr size_t MAX_URI_LENGTH          = 2048;    
    static constexpr size_t READ_BUFFER_SIZE        = 4096;    

    static std::optional<HttpRequest> parse(int client_fd);

    HttpMethod getMethod() const { return method_; }
    const std::string& getMethodString() const { return methodStr_; }
    const std::string& getUri() const { return uri_; }
    const std::string& getVersion() const { return version_; }
    const std::string& getBody() const { return body_; }
    const std::string& getQueryString() const { return queryString_; }
    const std::string& getPath() const { return path_; }
    std::string getHeader(const std::string& key) const;
    const std::unordered_map<std::string, std::string>& getHeaders() const {
        return headers_;
    }

    private:
    HttpMethod  method_;
    std::string methodStr_;
    std::string uri_;
    std::string path_;
    std::string queryString_;
    std::string version_;
    std::string body_;
    std::unordered_map<std::string, std::string> headers_;

    bool parseRequestLine(const std::string& line);
    bool parseHeaderLine(const std::string& line);
    static std::string urlDecode(const std::string& encoded);
    static std::string toLower(const std::string& str);
};

#endif // HTTP_REQUEST_HPP