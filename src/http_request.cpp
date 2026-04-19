#include "http_request.hpp"
#include "logger.hpp"

#include <sys/socket.h>
#include <sstream>
#include <algorithm>
#include <cctype>

std::optional<HttpRequest> HttpRequest::parse(int client_fd) {
    Logger& logger = Logger::getInstance();

    std::string rawData;
    char buffer[READ_BUFFER_SIZE];
    bool headersComplete = false;
    size_t totalRead = 0;

    while (!headersComplete) {
        ssize_t bytesRead = recv(client_fd, buffer, sizeof(buffer) - 1, 0);

        if (bytesRead <= 0) {
            logger.debug("Connection closed or read error during request parsing");
            return std::nullopt;
        }

        totalRead += static_cast<size_t>(bytesRead);

        if (totalRead > MAX_REQUEST_LINE_LENGTH * MAX_HEADER_COUNT + MAX_BODY_SIZE) {
            logger.warning("Request exceeds maximum allowed size — rejecting");
            return std::nullopt;
        }

        rawData.append(buffer, static_cast<size_t>(bytesRead));

        if (rawData.find("\r\n\r\n") != std::string::npos) {
            headersComplete = true;
        }
    }

    size_t headerEnd = rawData.find("\r\n\r\n");
    std::string headerSection = rawData.substr(0, headerEnd);
    std::string bodyData = rawData.substr(headerEnd + 4);

    HttpRequest request;
    std::istringstream headerStream(headerSection);
    std::string line;
    bool firstLine = true;

    while (std::getline(headerStream, line)) {

        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }

        if (firstLine) {
            if (!request.parseRequestLine(line)) {
                logger.warning("Malformed request line: " + line.substr(0, 100));
                return std::nullopt;
            }
            firstLine = false;
        } else {
            if (!line.empty()) {
                if (request.headers_.size() >= MAX_HEADER_COUNT) {
                    logger.warning("Too many headers — rejecting request");
                    return std::nullopt;
                }
                if (!request.parseHeaderLine(line)) {
                    logger.warning("Malformed header line — rejecting request");
                    return std::nullopt;
                }
            }
        }
    }

    if (request.method_ == HttpMethod::POST) {
        std::string contentLengthStr = request.getHeader("content-length");
        if (!contentLengthStr.empty()) {
            size_t contentLength = 0;
            try {
                contentLength = std::stoul(contentLengthStr);
            } catch (const std::exception&) {
                logger.warning("Invalid Content-Length header value");
                return std::nullopt;
            }

            if (contentLength > MAX_BODY_SIZE) {
                logger.warning("POST body too large: " + std::to_string(contentLength));
                return std::nullopt;
            }

            request.body_ = bodyData;

            while (request.body_.size() < contentLength) {
                ssize_t bytesRead = recv(client_fd, buffer,
                    std::min(sizeof(buffer) - 1,
                             contentLength - request.body_.size()), 0);
                if (bytesRead <= 0) {
                    logger.warning("Connection closed while reading POST body");
                    return std::nullopt;
                }
                request.body_.append(buffer, static_cast<size_t>(bytesRead));
            }

            request.body_ = request.body_.substr(0, contentLength);
        }
    }

    logger.info("Parsed " + request.methodStr_ + " request for: " + request.path_);
    return request;
}

bool HttpRequest::parseRequestLine(const std::string& line) {
    if (line.length() > MAX_REQUEST_LINE_LENGTH) {
        return false;
    }

    std::istringstream iss(line);
    if (!(iss >> methodStr_ >> uri_ >> version_)) {
        return false; 
    }

    std::string extra;
    if (iss >> extra) {
        return false;
    }

    if (methodStr_ == "GET") {
        method_ = HttpMethod::GET;
    } else if (methodStr_ == "POST") {
        method_ = HttpMethod::POST;
    } else {
        method_ = HttpMethod::UNSUPPORTED;
    }

    if (uri_.length() > MAX_URI_LENGTH) {
        return false;
    }

    if (uri_.find('\0') != std::string::npos) {
        return false;
    }

    if (version_.substr(0, 5) != "HTTP/") {
        return false;
    }

    size_t queryPos = uri_.find('?');
    if (queryPos != std::string::npos) {
        path_ = urlDecode(uri_.substr(0, queryPos));
        queryString_ = uri_.substr(queryPos + 1);
    } else {
        path_ = urlDecode(uri_);
    }

    return true;
}

bool HttpRequest::parseHeaderLine(const std::string& line) {
    size_t colonPos = line.find(':');
    if (colonPos == std::string::npos || colonPos == 0) {
        return false;
    }

    std::string key   = toLower(line.substr(0, colonPos));
    std::string value = line.substr(colonPos + 1);

    size_t start = value.find_first_not_of(" \t");
    if (start != std::string::npos) {
        value = value.substr(start);
    }

    size_t end = value.find_last_not_of(" \t\r\n");
    if (end != std::string::npos) {
        value = value.substr(0, end + 1);
    }

    if (value.find('\r') != std::string::npos || value.find('\n') != std::string::npos) {
        Logger::getInstance().warning("Header injection attempt detected in: " + key);
        return false;
    }

    headers_[key] = value;
    return true;
}

std::string HttpRequest::urlDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            if (std::isxdigit(encoded[i + 1]) && std::isxdigit(encoded[i + 2])) {
                std::string hex = encoded.substr(i + 1, 2);
                char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));

                if (decodedChar == '\0') {
                    continue;
                }

                decoded += decodedChar;
                i += 2;
            } else {
                decoded += encoded[i];
            }
        } else if (encoded[i] == '+') {
            decoded += ' ';
        } else {
            decoded += encoded[i];
        }
    }

    return decoded;
}

std::string HttpRequest::getHeader(const std::string& key) const {
    std::string lowerKey = toLower(key);
    auto it = headers_.find(lowerKey);
    if (it != headers_.end()) {
        return it->second;
    }
    return "";
}

std::string HttpRequest::toLower(const std::string& str) {
    std::string lower = str;
    std::transform(lower.begin(), lower.end(), lower.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return lower;
}