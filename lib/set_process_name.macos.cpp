#ifndef __APPLE__
#error "This file is only meant to be compiled on macOS"
#else

#include "utils/set_process_name.hpp"
#include <everest/logging.hpp>
#include <fmt/core.h>

void set_process_name(const std::string& name) {
    EVLOG_verbose << fmt::format("Setting process name to: '{}'...", name);

    EVLOG_warning << fmt::format("Could not set process name to '{}'. Not supported on macos", name);
}

#endif // __APPLE__