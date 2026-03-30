#ifndef SOCKET_WRAPPER_HPP
#define SOCKET_WRAPPER_HPP

#include <unistd.h>   // close()
#include <utility>

class SocketWrapper {
public:
    // Constructor: take ownership of a file descriptor
    // "explicit" prevents accidental conversions like: SocketWrapper s = 5;
    explicit SocketWrapper(int fd = -1) : fd_(fd) {}
    // Destructor: automatically close the socket when this object dies
    ~SocketWrapper() {
        if (fd_ >= 0) {
            ::close(fd_);
        }
    }
    // Move constructor: steal the fd from another wrapper
    SocketWrapper(SocketWrapper&& other) noexcept
        : fd_(std::exchange(other.fd_, -1)) {}

    // Move assignment: release our fd, take theirs
    SocketWrapper& operator=(SocketWrapper&& other) noexcept {
        if (this != &other) {
            if (fd_ >= 0) {
                ::close(fd_);
            }
            fd_ = std::exchange(other.fd_, -1);
        }
        return *this;
    }

    // DELETE copy — two wrappers closing the same fd = disaster
    SocketWrapper(const SocketWrapper&) = delete;
    SocketWrapper& operator=(const SocketWrapper&) = delete;

    // Get the raw fd number for use in system calls like send() and recv()
    int get() const { return fd_; }

    // Check if this wrapper actually holds a valid fd
    bool isValid() const { return fd_ >= 0; }

    // Give up ownership without closing (use sparingly)
    int release() {
        return std::exchange(fd_, -1);
    }

private:
    int fd_;  // The file descriptor we own. -1 = empty.
};

#endif // SOCKET_WRAPPER_HPP