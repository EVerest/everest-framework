// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2022 Pionix GmbH and Contributors to EVerest
#include <csignal>
#include <everest/logging.hpp>

namespace Everest {
namespace ExitHandler {

void exit_handler(const int sig_num){
    /* do clean shutdown (exit status 0) */
    EVLOG_info << "Received SIGINT/SIGTERM (exit status: " << sig_num << ") and shutting down";
    exit(EXIT_SUCCESS);
}

} // namespace ExitHandler
} // namespace Everest




