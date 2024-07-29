// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "misc.hpp"

#include <cstdlib>
#include <stdexcept>

static std::string get_ev_prefix_from_env() {
    const auto prefix = std::getenv("EV_PREFIX");
    if (prefix == nullptr) {
        throw std::runtime_error("EV_PREFIX needed for everestpy");
    }

    return prefix;
}

static std::string get_ev_log_conf_file_from_env() {
    const auto logging_config_file = std::getenv("EV_LOG_CONF_FILE");
    if (logging_config_file == nullptr) {
        throw std::runtime_error("EV_LOG_CONF_FILE needed for everestpy");
    }

    return logging_config_file;
}

static std::string get_ev_module_from_env() {
    const auto module_id = std::getenv("EV_MODULE");
    if (module_id == nullptr) {
        throw std::runtime_error("EV_MODULE needed for everestpy");
    }

    return module_id;
}

static std::shared_ptr<Everest::MQTTSettings> get_mqtt_settings_from_env() {
    const auto mqtt_everest_prefix = std::getenv("EV_MQTT_EVEREST_PREFIX");
    const auto mqtt_external_prefix = std::getenv("EV_MQTT_EXTERNAL_PREFIX");
    const auto mqtt_broker_socket_path = std::getenv("EV_MQTT_BROKER_SOCKET_PATH");
    const auto mqtt_broker_host = std::getenv("EV_MQTT_BROKER_HOST");
    const auto mqtt_broker_port = std::getenv("EV_MQTT_BROKER_PORT");

    std::shared_ptr<Everest::MQTTSettings> mqtt_settings;
    if (mqtt_broker_socket_path == nullptr) {
        if (mqtt_broker_host == nullptr or mqtt_broker_port == nullptr) {
            throw std::runtime_error("If EV_MQTT_BROKER_SOCKET_PATH is not set EV_MQTT_BROKER_HOST and "
                                     "EV_MQTT_BROKER_PORT are needed for everestpy");
        }
        auto mqtt_broker_port_ = Everest::defaults::MQTT_BROKER_PORT;
        try {
            mqtt_broker_port_ = std::stoi(mqtt_broker_port);
        } catch (...) {
            EVLOG_warning << "Could not parse MQTT broker port, using default: " << mqtt_broker_port_;
        }
        mqtt_settings = std::make_shared<Everest::MQTTSettings>(mqtt_broker_host, mqtt_broker_port_,
                                                                mqtt_everest_prefix, mqtt_external_prefix);
    } else {
        mqtt_settings =
            std::make_shared<Everest::MQTTSettings>(mqtt_broker_socket_path, mqtt_everest_prefix, mqtt_external_prefix);
    }

    return mqtt_settings;
}

// just for compatibility...
RuntimeSession::RuntimeSession(const std::string& prefix, const std::string& config_file) : RuntimeSession() {
    EVLOG_info << "called the old RuntimeSession ctor";
}

// old ctor
// RuntimeSession::RuntimeSession() : RuntimeSession(get_ev_prefix_from_env(), get_ev_conf_file_from_env()) {
// }

RuntimeSession::RuntimeSession() {
    auto module_id = get_ev_module_from_env();

    // TODO: get the rest of the parameters from env

    EVLOG_info << "new RuntimeSession ctor: " << module_id;
    ;

    // FIXME: proper logging path...
    namespace fs = std::filesystem;
    fs::path logging_config_file = Everest::assert_file(get_ev_log_conf_file_from_env(), "Default logging config");
    Everest::Logging::init(logging_config_file.string(), module_id);

    this->mqtt_settings = get_mqtt_settings_from_env();

    EVLOG_error << "calling get_config() for PY module: " << module_id;
    auto result = Everest::ModuleConfig::get_config(mqtt_settings, module_id);
    this->rs = std::make_shared<Everest::RuntimeSettings>(result.at("settings"));
    this->config = std::make_unique<Everest::Config>(mqtt_settings, result);
}

ModuleSetup create_setup_from_config(const std::string& module_id, Everest::Config& config) {
    ModuleSetup setup;

    const std::string& module_name = config.get_main_config().at(module_id).at("module");
    auto module_manifest = config.get_manifests().at(module_name);

    // setup connections
    for (auto& requirement : module_manifest.at("requires").items()) {

        auto const& requirement_id = requirement.key();

        json req_route_list = config.resolve_requirement(module_id, requirement_id);
        // if this was a requirement with min_connections == 1 and max_connections == 1,
        // this will be simply a single connection, but an array of connections otherwise
        // (this array can have only one entry, if only one connection was provided, though)
        const bool is_list = req_route_list.is_array();
        if (!is_list) {
            req_route_list = json::array({req_route_list});
        }

        auto fulfillment_list_it = setup.connections.insert({requirement_id, {}}).first;
        auto& fulfillment_list = fulfillment_list_it->second;
        fulfillment_list.reserve(req_route_list.size());

        for (size_t i = 0; i < req_route_list.size(); i++) {
            const auto& req_route = req_route_list[i];
            auto fulfillment = Fulfillment{req_route["module_id"], req_route["implementation_id"], {requirement_id, i}};
            fulfillment_list.emplace_back(std::move(fulfillment));
        }
    }

    const auto& config_maps = config.get_module_json_config(module_id);

    for (const auto& config_map : config_maps.items()) {
        const auto& impl_id = config_map.key();
        if (impl_id == "!module") {
            setup.configs.module = config_map.value();
            continue;
        }

        setup.configs.implementations.emplace(impl_id, config_map.value());
    }

    return setup;
}

Interface create_everest_interface_from_definition(const json& def) {
    Interface intf;
    if (def.contains("cmds")) {
        const auto& cmds = def.at("cmds");
        intf.commands.reserve(cmds.size());

        for (const auto& cmd : cmds.items()) {
            intf.commands.push_back(cmd.key());
        }
    }

    if (def.contains("vars")) {
        const auto& vars = def.at("vars");
        intf.variables.reserve(vars.size());

        for (const auto& var : vars.items()) {
            intf.variables.push_back(var.key());
        }
    }

    if (def.contains("errors")) {
        const auto& errors = def.at("errors");

        int errors_size = 0;
        for (const auto& error_namespace_it : errors.items()) {
            errors_size += error_namespace_it.value().size();
        }
        intf.errors.reserve(errors_size);

        for (const auto& error_namespace_it : errors.items()) {
            for (const auto& error_name_it : error_namespace_it.value().items()) {
                intf.errors.push_back(error_namespace_it.key() + "/" + error_name_it.key());
            }
        }
    }

    return intf;
}
