#ifndef __APPLE__
#error "This file is only meant to be compiled on macOS"
#else

#include <fmt/core.h>
#include <stdexcept>

#include <fcntl.h>
#include <unistd.h>

void create_pipe(int* pipefd) {
    if (pipe(pipefd)) {
        throw std::runtime_error(fmt::format("Syscall pipe() failed ({}), exiting", strerror(errno)));
    }
    if (fcntl(pipefd[0], F_NOCACHE, 1)) {
        throw std::runtime_error(fmt::format("Syscall fcntl() failed ({}), exiting", strerror(errno)));
    }

    if (fcntl(pipefd[0], F_SETFD, O_CLOEXEC)) {
        throw std::runtime_error(fmt::format("Syscall fcntl() failed ({}), exiting", strerror(errno)));
    }
}
#endif // __APPLE__