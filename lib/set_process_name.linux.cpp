#ifndef __linux__
#error "This file is only meant to be compiled on Linux"
#else

#include "utils/set_process_name.hpp"
#include <everest/logging.hpp>
#include <fmt/core.h>
#include <sys/prctl.h>

void set_process_name(const std::string& name) {
    EVLOG_verbose << fmt::format("Setting process name to: '{}'...", name);

    if (prctl(PR_SET_NAME, name.c_str())) {
        EVLOG_warning << fmt::format("Could not set process name to '{}'", name);
    }
}
#endif // __linux__
