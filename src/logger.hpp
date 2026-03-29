/**
 * @file logger.hpp
 * @brief Logger class for logging messages to a file
 * @author Waterspecial (boluwatifeomirinde080@gmail.com)
 * @version 0.1
 * @date 2026-03-29
 * 
 * @copyright Copyright (c) 2026
 */
#ifndef LOGGER_HPP
#define LOGGER_HPP

#include <string>
#include <fstream>
#include <mutex>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <sstream>

enum class LogLevel {
    DEBUG    = 0,
    INFO     = 1,
    WARNING  = 2,
    ERROR    = 3,
    CRITICAL = 4
};
class Logger {
public:
    // Singleton: get the one-and-only Logger instance
    static Logger& getInstance() {
        static Logger instance;
        return instance;
    }

    bool init(const std::string& filepath, LogLevel minLevel = LogLevel::INFO) {
        std::lock_guard<std::mutex> lock(mutex_);
        minLevel_ = minLevel;
        logFile_.open(filepath, std::ios::app);
        if (!logFile_.is_open()) {
            std::cerr << "[LOGGER] Failed to open log file: " << filepath << std::endl;
            return false;
        }
        return true;
    }

    void log(LogLevel level, const std::string& message) {
        if (level < minLevel_) return;

        std::lock_guard<std::mutex> lock(mutex_);

        std::string timestamp = getTimestamp();
        std::string levelStr  = levelToString(level);
        std::string formatted = "[" + timestamp + "] [" + levelStr + "] " + message;

        if (logFile_.is_open()) {
            logFile_ << formatted << std::endl;
            logFile_.flush();
        }

        if (level >= LogLevel::ERROR) {
            std::cerr << formatted << std::endl;
        } else {
            std::cout << formatted << std::endl;
        }
    }

    void debug(const std::string& msg)    { log(LogLevel::DEBUG, msg); }
    void info(const std::string& msg)     { log(LogLevel::INFO, msg); }
    void warning(const std::string& msg)  { log(LogLevel::WARNING, msg); }
    void error(const std::string& msg)    { log(LogLevel::ERROR, msg); }
    void critical(const std::string& msg) { log(LogLevel::CRITICAL, msg); }

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    Logger(Logger&&) = delete;
    Logger& operator=(Logger&&) = delete;

    private:
    Logger() : minLevel_(LogLevel::INFO) {}

    ~Logger() {
        if (logFile_.is_open()) {
            log(LogLevel::INFO, "Logger shutting down");
        }
    }

    std::string getTimestamp() {
        auto now = std::time(nullptr);
        auto* tm = std::localtime(&now);
        std::ostringstream oss;
        oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
        return oss.str();
    }

    std::string levelToString(LogLevel level) {
        switch (level) {
            case LogLevel::DEBUG:    return "DEBUG   ";
            case LogLevel::INFO:     return "INFO    ";
            case LogLevel::WARNING:  return "WARNING ";
            case LogLevel::ERROR:    return "ERROR   ";
            case LogLevel::CRITICAL: return "CRITICAL";
            default:                 return "UNKNOWN ";
        }
    }

    std::ofstream logFile_;
    std::mutex    mutex_;
    LogLevel      minLevel_;
};

#endif // LOGGER_HPP