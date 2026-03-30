#include "sandbox.hpp"
#include "logger.hpp"

// seccomp is Linux-only — conditionally include
#ifdef HAS_SECCOMP
    #include <seccomp.h>
    #define SECCOMP_AVAILABLE 1
#else
    #define SECCOMP_AVAILABLE 0
#endif

#ifdef __linux__
    #include <sys/prctl.h>
#endif

#include <unistd.h>
#include <cerrno>
#include <cstring>

void Sandbox::applySandbox() {
    Logger& logger = Logger::getInstance();

#if SECCOMP_AVAILABLE
    logger.info("Applying seccomp sandbox to child process (PID: "
                + std::to_string(getpid()) + ")");
    applySeccomp();
#else
    logger.warning("Seccomp not available on this platform — "
                   "sandbox not applied (development mode). "
                   "Deploy on Linux/Fedora for full sandboxing.");

    // Even without seccomp, apply what we can on Linux
    #ifdef __linux__
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
        logger.warning("Failed to set PR_SET_NO_NEW_PRIVS: "
                      + std::string(strerror(errno)));
    }
    #endif
#endif
}

#if SECCOMP_AVAILABLE
void Sandbox::applySeccomp() {
    Logger& logger = Logger::getInstance();

    // Prevent gaining new privileges (required before seccomp)
    if (prctl(PR_SET_NO_NEW_PRIVS, 1, 0, 0, 0) == -1) {
        logger.error("Failed to set PR_SET_NO_NEW_PRIVS: "
                    + std::string(strerror(errno)));
        return;
    }

    // Create filter: default action = KILL the process
    // Any syscall not explicitly allowed will kill this process instantly
    scmp_filter_ctx ctx = seccomp_init(SCMP_ACT_KILL_PROCESS);
    if (ctx == nullptr) {
        logger.error("Failed to initialise seccomp context");
        return;
    }

    // WHITELIST: only these syscalls are allowed

    // File I/O (needed to save form data)
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(read), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(write), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(open), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(openat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(close), 0);

    // File metadata (needed by C library)
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(fstat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(stat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lstat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(newfstatat), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(lseek), 0);

    // Memory management (needed by C++ runtime)
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(munmap), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(brk), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(mprotect), 0);

    // Process termination
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(exit_group), 0);

    // Signal handling (needed by C++ exceptions)
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigreturn), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigaction), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(rt_sigprocmask), 0);

    // Time (needed for timestamp in filenames)
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(clock_gettime), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(gettimeofday), 0);

    // Misc (needed by glibc internally)
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getpid), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(getrandom), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(futex), 0);
    seccomp_rule_add(ctx, SCMP_ACT_ALLOW, SCMP_SYS(writev), 0);

    // Load the filter into the kernel
    if (seccomp_load(ctx) < 0) {
        logger.error("Failed to load seccomp filter: " + std::string(strerror(errno)));
        seccomp_release(ctx);
        return;
    }

    seccomp_release(ctx);
    logger.info("Seccomp sandbox applied successfully");
}
#endif