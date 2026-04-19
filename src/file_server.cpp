#include "file_server.hpp"
#include "logger.hpp"

#include <fstream>
#include <sstream>
#include <climits>     
#include <cstdlib>    
#include <algorithm>
#include <sys/stat.h> 

FileServer::FileServer(const std::string& webRoot) {
    char resolvedRoot[PATH_MAX];
    if (realpath(webRoot.c_str(), resolvedRoot) != nullptr) {
        webRoot_ = std::string(resolvedRoot);
    } else {
        webRoot_ = webRoot;
        Logger::getInstance().error("Failed to resolve web root path: " + webRoot);
    }
    Logger::getInstance().info("FileServer web root: " + webRoot_);
}

HttpResponse FileServer::serveFile(const std::string& requestPath) {
    Logger& logger = Logger::getInstance();

    // Resolve the path securely
    std::string resolvedPath;
    if (!resolvePath(requestPath, resolvedPath)) {
        logger.info("File not found or access denied: " + requestPath);
        return HttpResponse::notFound();
    }

    std::string content;
    if (!readFile(resolvedPath, content)) {
        logger.error("Failed to read file: " + resolvedPath);
        return HttpResponse::internalError();
    }

    std::string ext = getExtension(resolvedPath);
    std::string mimeType = HttpResponse::getMimeType(ext);

    // Build response
    logger.info("Serving file: " + resolvedPath + " (" + mimeType + ", "
                + std::to_string(content.size()) + " bytes)");
    return HttpResponse::ok(content, mimeType);
}

bool FileServer::resolvePath(const std::string& requestPath, std::string& resolvedPath) {
    Logger& logger = Logger::getInstance();

    if (requestPath.find("..") != std::string::npos) {
        logger.warning("SECURITY: Path traversal attempt detected: " +
            requestPath.substr(0, 200));
        return false;
    }

    if (requestPath.find('\0') != std::string::npos) {
        logger.warning("SECURITY: Null byte in request path");
        return false;
    }

    std::string fullPath = webRoot_;
    if (requestPath.empty() || requestPath == "/") {
        fullPath += "/index.html";
    } else {
        fullPath += requestPath;
    }

    // Canonicalise with realpath()
    char canonicalPath[PATH_MAX];
    if (realpath(fullPath.c_str(), canonicalPath) == nullptr) {
        return false;
    }

    resolvedPath = std::string(canonicalPath);

    if (resolvedPath.substr(0, webRoot_.length()) != webRoot_) {
        logger.warning("SECURITY: Path escapes web root! Requested: " +
            requestPath + " → Resolved: " + resolvedPath);
        return false;
    }

    struct stat fileStat;
    if (stat(resolvedPath.c_str(), &fileStat) != 0 || !S_ISREG(fileStat.st_mode)) {
        if (S_ISDIR(fileStat.st_mode)) {
            std::string indexPath = resolvedPath + "/index.html";
            if (realpath(indexPath.c_str(), canonicalPath) != nullptr) {
                resolvedPath = std::string(canonicalPath);
                if (resolvedPath.substr(0, webRoot_.length()) != webRoot_) {
                    return false;
                }
                return true;
            }
        }
        return false;
    }
    return true;
}

bool FileServer::readFile(const std::string& filepath, std::string& content) {
    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open()) {
        return false;
    }

    std::ostringstream oss;
    oss << file.rdbuf();
    content = oss.str();

    return true;
}

std::string FileServer::getExtension(const std::string& filepath) {
    size_t dotPos = filepath.rfind('.');
    if (dotPos != std::string::npos) {
        std::string ext = filepath.substr(dotPos);
        std::transform(ext.begin(), ext.end(), ext.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return ext;
    }
    return "";
}