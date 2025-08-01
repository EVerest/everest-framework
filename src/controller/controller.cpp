// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <cstdlib>
#include <thread>

#include <fmt/format.h>
#include <nlohmann/json.hpp>

#include <everest/logging.hpp>

#include "command_api.hpp"
#include "ipc.hpp"
#include "rpc.hpp"
#include "server.hpp"

using json = nlohmann::json;

namespace {
int run_controller() {
    auto socket_fd = STDIN_FILENO;

    const auto message = Everest::controller_ipc::receive_message(socket_fd);

    if (message.status != Everest::controller_ipc::MESSAGE_RETURN_STATUS::OK) {
        throw std::runtime_error("Controller process could not read initial config message");
    }

    // FIXME (aw): validation
    const auto config_params = message.json.at("params");

    Everest::Logging::init(config_params.at("logging_config_file"), "everest_ctrl");

    EVLOG_debug << "everest controller process started ...";

    const CommandApi::Config config{
        config_params.at("module_dir"),
        config_params.at("interface_dir"),
        config_params.at("configs_dir"),
        config_params.at("controller_rpc_timeout_ms"),
    };

    RPC rpc(socket_fd, config);
    Server backend;
    const int controller_port = config_params.at("controller_port").get<int>();

    // FIXME (aw): don't use hard-coded path!
    std::thread(
        &Server::run, &backend, [&rpc](const nlohmann::json& request) { return rpc.handle_json_rpc(request); },
        config_params.at("www_dir"), controller_port)
        .detach();

    while (true) {
        rpc.run([&backend](const nlohmann::json& notification) { backend.push(notification); });
    }

    return 0;
}
} // namespace

int main([[maybe_unused]] int argc, char* argv[]) {
    const auto argv0 = *argv;
    if (strcmp(argv0, MAGIC_CONTROLLER_ARG0) != 0) {
        fmt::print(stderr, "This binary does not yet support to be started manually\n");
        return EXIT_FAILURE;
    }

    try {
        return run_controller();
    } catch (...) {
        return EXIT_FAILURE;
    }
}
