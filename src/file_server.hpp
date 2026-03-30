#ifndef FILE_SERVER_HPP
#define FILE_SERVER_HPP

#include "http_response.hpp"
#include <string>

class FileServer {
public:
    // Constructor: takes the path to the web root directory
    explicit FileServer(const std::string& webRoot);

    // Serve a file based on the requested URL path
    // Returns a 200 response with file content, or 404/403 on failure
    HttpResponse serveFile(const std::string& requestPath);

private:
    std::string webRoot_;  // Canonical path to web root

    // SECURITY: validates and resolves a path safely
    bool resolvePath(const std::string& requestPath, std::string& resolvedPath);

    // Read file contents into a string
    bool readFile(const std::string& filepath, std::string& content);

    // Get file extension (e.g. ".html" from "/page.html")
    std::string getExtension(const std::string& filepath);
};

#endif // FILE_SERVER_HPP