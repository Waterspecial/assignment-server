#ifndef FILE_SERVER_HPP
#define FILE_SERVER_HPP

#include "http_response.hpp"
#include <string>

class FileServer {
public:
    explicit FileServer(const std::string& webRoot);
    HttpResponse serveFile(const std::string& requestPath);

private:
    std::string webRoot_;

    bool resolvePath(const std::string& requestPath, std::string& resolvedPath);

    bool readFile(const std::string& filepath, std::string& content);

    std::string getExtension(const std::string& filepath);
};

#endif