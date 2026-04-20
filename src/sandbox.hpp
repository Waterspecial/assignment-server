#ifndef SANDBOX_HPP
#define SANDBOX_HPP

class Sandbox {
public:
    static void applySandbox();

private:
    static void applySeccomp();
};

#endif