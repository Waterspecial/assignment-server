#ifndef SANDBOX_HPP
#define SANDBOX_HPP

class Sandbox {
public:
    // On Linux: applies seccomp syscall filter
    // On macOS: logs a warning (development mode)
    static void applySandbox();

private:
    static void applySeccomp();
};

#endif