#ifndef SOCKET_WRAPPER_HPP
#define SOCKET_WRAPPER_HPP

#include <unistd.h>   // close()
#include <utility>

class SocketWrapper {
public:
    
    explicit SocketWrapper(int fd = -1) : fd_(fd) {}
    ~SocketWrapper() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    SocketWrapper(SocketWrapper&& other) noexcept
        : fd_(std::exchange(other.fd_, -1)) {}
    SocketWrapper& operator=(SocketWrapper&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;

    int get() const { return fd_; }

    bool isValid() const { return fd_ >= 0; }

    int release() {
        return std::exchange(fd_, -1);
    }
private:
    int fd_;
};

#endif