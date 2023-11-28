// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest

#include "system_unix.hpp"

#include <cassert>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <thread>

#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <signal.h>
#include <unistd.h>

#include <fmt/core.h>

namespace Everest::system {

const auto PARENT_DIED_SIGNAL = SIGTERM;

bool keep_caps() {
    return true;
}

std::string set_caps(const std::vector<std::string>& capabilities) {
    std::cerr << "Setting capabilities is not supported on MacOS" << std::endl;
    return {};
}

std::string set_real_user(const std::string& user_name) {
    // Set special capabilities if required by module
    std::cerr << "Setting real user is not supported on MacOS" << std::endl;
    return {};
}

void SubProcess::send_error_and_exit(const std::string& message) {
    // FIXME (aw): howto do asserts?
    assert(pid == 0);

    write(fd, message.c_str(), std::min(message.size(), MAX_PIPE_MESSAGE_SIZE - 1));
    close(fd);
    _exit(EXIT_FAILURE);
}

// FIXME (aw): this function should be callable only once
pid_t SubProcess::check_child_executed() {
    assert(pid != 0);

    std::string message(MAX_PIPE_MESSAGE_SIZE, 0);

    auto retval = read(fd, message.data(), MAX_PIPE_MESSAGE_SIZE);
    if (retval == -1) {
        throw std::runtime_error(fmt::format(
            "Failed to communicate via pipe with forked child process. Syscall to read() failed ({}), exiting",
            strerror(errno)));
    } else if (retval > 0) {
        throw std::runtime_error(fmt::format("Forked child process did not complete exec():\n{}", message.c_str()));
    }

    close(fd);
    return pid;
}

SubProcess SubProcess::create(bool set_pdeathsig) {
    int pipefd[2];

    if (pipe(pipefd)) {
        throw std::runtime_error(fmt::format("Syscall pipe2() failed ({}), exiting", strerror(errno)));
    }

    const auto reading_end_fd = pipefd[0];
    const auto writing_end_fd = pipefd[1];

    const auto parent_pid = getpid();
    std::cerr << "Parent pid: " << parent_pid << std::endl;

    pid_t pid = fork();
    std::cerr << "Forked pid: " << pid << std::endl;

    if (pid == -1) {
        throw std::runtime_error(fmt::format("Syscall fork() failed ({}), exiting", strerror(errno)));
    }

    if (pid == 0) {
        // close read end in child
        close(reading_end_fd);
        SubProcess handle{writing_end_fd, pid};

        if (set_pdeathsig) {
            pid_t fork2_pid = fork();
            if (fork2_pid == -1) {
                handle.send_error_and_exit(fmt::format("Syscall fork() failed ({}), exiting", strerror(errno)));
            }
            if (fork2_pid == 0) {
                // We are in the second forked process.
                return handle;
            } else {
                // We are in the first forked process. This one will be used
                // to monitor the parent process. And would kill the second
                // forked process if the parent process dies.
                close(writing_end_fd);
                for (;;) {
                    const auto current_parent_pid = getppid();
                    if (current_parent_pid != parent_pid) {
                        // kill ourself, with the same handler as we would have
                        // happened when the parent process died
                        kill(fork2_pid, PARENT_DIED_SIGNAL);
                        waitpid(fork2_pid, nullptr, 0);
                        _exit(0);
                    }
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }
            }
        }

    } else {
        std::cout << "Parent process" << std::endl;
        close(writing_end_fd);
        return {reading_end_fd, pid};
    }
}

} // namespace Everest::system
