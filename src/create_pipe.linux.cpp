#ifndef __linux__
#error "This file is only meant to be compiled on Linux"
#else

#include "create_pipe.hpp"
#include <fcntl.h>
#include <fmt/format.h>
#include <stdexcept>
#include <unistd.h>

void create_pipe(int* pipefd) {
    if (pipe2(pipefd, O_CLOEXEC | O_DIRECT)) {
        throw std::runtime_error(fmt::format("Syscall pipe2() failed ({}), exiting", strerror(errno)));
    }
}
#endif // __linux__