#include "form_handler.hpp"
#include "logger.hpp"
#include "sandbox.hpp"

#include <unistd.h>     // fork(), pipe(), read(), write()
#include <sys/wait.h>   // waitpid()
#include <fstream>
#include <sstream>
#include <ctime>
#include <cstdlib>
#include <algorithm>
#include <cctype>
#include <cstring>

FormHandler::FormHandler(const std::string& storageDir) : storageDir_(storageDir) {}

HttpResponse FormHandler::handleForm(const HttpRequest& request, int /*clientFd*/) {
    Logger& logger = Logger::getInstance();

    // Validate Content-Type
    std::string contentType = request.getHeader("content-type");
    if (contentType.find("application/x-www-form-urlencoded") == std::string::npos) {
        logger.warning("Unsupported form content type: " + contentType);
        return HttpResponse::badRequest("Unsupported content type.");
    }

    // Check body is not empty
    if (request.getBody().empty()) {
        logger.warning("Empty POST body received");
        return HttpResponse::badRequest("Empty form submission.");
    }

    // Create a pipe for child → parent communication
    int pipefd[2];
    if (pipe(pipefd) == -1) {
        logger.error("Failed to create pipe: " + std::string(strerror(errno)));
        return HttpResponse::internalError();
    }

    // Fork a child process
    pid_t pid = fork();

    if (pid < 0) {
        // Fork failed
        logger.error("Failed to fork: " + std::string(strerror(errno)));
        close(pipefd[0]);
        close(pipefd[1]);
        return HttpResponse::internalError();

    } else if (pid == 0) {
        // ============================================
        // CHILD PROCESS — isolated from main server
        // ============================================

        // Close read end (child only writes)
        close(pipefd[0]);

        // SECURITY: apply sandbox BEFORE processing untrusted data
        Sandbox::applySandbox();

        // Parse form data
        auto formData = parseFormData(request.getBody());

        // Sanitise all values
        for (auto& [key, value] : formData) {
            value = sanitise(value);
        }

        // Save to disk
        bool success = saveFormData(formData);

        // Report result to parent via pipe
        const char* result = success ? "OK" : "FAIL";
        ssize_t written = write(pipefd[1], result, strlen(result));
        (void)written;

        close(pipefd[1]);

        // CRITICAL: use _exit() not exit()
        // exit() would run the parent's cleanup handlers (bad!)
        // _exit() terminates immediately and safely
        _exit(success ? 0 : 1);

    } else {
        // ============================================
        // PARENT PROCESS — waits for child result
        // ============================================

        // Close write end (parent only reads)
        close(pipefd[1]);

        // Wait for child to finish
        int status;
        pid_t result = waitpid(pid, &status, 0);

        // Read result from pipe
        char resultBuf[16] = {0};
        ssize_t bytesRead = read(pipefd[0], resultBuf, sizeof(resultBuf) - 1);
        close(pipefd[0]);

        if (result < 0) {
            logger.error("waitpid failed for form handler child");
            return HttpResponse::internalError();
        }

        // Check if child succeeded
        if (WIFEXITED(status) && WEXITSTATUS(status) == 0
            && bytesRead > 0 && std::string(resultBuf) == "OK") {
            logger.info("Form processed by child PID: " + std::to_string(pid));

            HttpResponse resp;
            resp.setStatus(200, "OK");
            resp.setHeader("Content-Type", "text/html; charset=utf-8");
            resp.setBody(
                "<!DOCTYPE html><html><head><title>Form Submitted</title></head>"
                "<body><h1>Thank You!</h1>"
                "<p>Your form submission has been received and saved.</p>"
                "<a href=\"/form.html\">Submit another</a>"
                "</body></html>"
            );
            return resp;
        } else {
            logger.error("Form handler child failed (PID: " + std::to_string(pid) + ")");
            return HttpResponse::internalError();
        }
    }
}

std::unordered_map<std::string, std::string> FormHandler::parseFormData(const std::string& body) {
    std::unordered_map<std::string, std::string> data;
    std::istringstream stream(body);
    std::string pair;

    while (std::getline(stream, pair, '&')) {
        size_t eqPos = pair.find('=');
        if (eqPos != std::string::npos) {
            std::string key   = urlDecode(pair.substr(0, eqPos));
            std::string value = urlDecode(pair.substr(eqPos + 1));

            // Limit field sizes
            if (key.length() <= 256 && value.length() <= 8192) {
                data[key] = value;
            } else {
                Logger::getInstance().warning("Form field too large, skipping: " +
                    key.substr(0, 50));
            }
        }
    }

    return data;
}

std::string FormHandler::sanitise(const std::string& input) {
    std::string output;
    output.reserve(input.length());

    for (char c : input) {
        switch (c) {
            // HTML entity encoding — prevents stored XSS
            case '<':  output += "&lt;";   break;
            case '>':  output += "&gt;";   break;
            case '&':  output += "&amp;";  break;
            case '"':  output += "&quot;"; break;
            case '\'': output += "&#39;";  break;
            default:
                // Remove control characters (prevents log injection)
                if (c >= 0x20 || c == '\t' || c == '\n' || c == '\r') {
                    output += c;
                }
                break;
        }
    }

    return output;
}

bool FormHandler::saveFormData(const std::unordered_map<std::string, std::string>& data) {
    std::string filename = generateFilename();
    std::string filepath = storageDir_ + "/" + filename;

    std::ofstream file(filepath);
    if (!file.is_open()) {
        Logger::getInstance().error("Failed to open form data file: " + filepath);
        return false;
    }

    // Write timestamp
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);
    char timeBuf[64];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y-%m-%d %H:%M:%S", tm);
    file << "[Submitted: " << timeBuf << "]\n";
    file << "---\n";

    // Write each field
    for (const auto& [key, value] : data) {
        file << key << ": " << value << "\n";
    }

    file.close();
    Logger::getInstance().info("Form data saved to: " + filepath);
    return true;
}

std::string FormHandler::generateFilename() {
    auto now = std::time(nullptr);
    auto* tm = std::localtime(&now);

    char timeBuf[32];
    std::strftime(timeBuf, sizeof(timeBuf), "%Y%m%d_%H%M%S", tm);

    srand(static_cast<unsigned>(now) ^ static_cast<unsigned>(getpid()));
    int randSuffix = rand() % 10000;

    return "form_" + std::string(timeBuf) + "_" + std::to_string(randSuffix) + ".txt";
}

std::string FormHandler::urlDecode(const std::string& encoded) {
    std::string decoded;
    decoded.reserve(encoded.length());

    for (size_t i = 0; i < encoded.length(); ++i) {
        if (encoded[i] == '%' && i + 2 < encoded.length()) {
            if (std::isxdigit(encoded[i + 1]) && std::isxdigit(encoded[i + 2])) {
                std::string hex = encoded.substr(i + 1, 2);
                char decodedChar = static_cast<char>(std::stoi(hex, nullptr, 16));
                if (decodedChar != '\0') {
                    decoded += decodedChar;
                }
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