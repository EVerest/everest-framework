// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include "misc.hpp"

#include <cstddef>
#include <cstdlib>
#include <stdexcept>

#include <utils/filesystem.hpp>

const std::string get_variable_from_env(const std::string& variable) {
    const auto value = std::getenv(variable.c_str());
    if (value == nullptr) {
        throw std::runtime_error(variable + " needed for everestpy");
    }

    return value;
}

const std::string get_variable_from_env(const std::string& variable, const std::string& default_value) {
    const auto value = std::getenv(variable.c_str());
    if (value == nullptr) {
        return default_value;
    }

    return value;
}

static Everest::MQTTSettings get_mqtt_settings_from_env() {
    const auto mqtt_everest_prefix =
        get_variable_from_env(Everest::EV_MQTT_EVEREST_PREFIX, Everest::defaults::MQTT_EVEREST_PREFIX);
    const auto mqtt_external_prefix =
        get_variable_from_env(Everest::EV_MQTT_EXTERNAL_PREFIX, Everest::defaults::MQTT_EXTERNAL_PREFIX);
    const auto mqtt_broker_socket_path = std::getenv(Everest::EV_MQTT_BROKER_SOCKET_PATH);
    const auto mqtt_broker_host = std::getenv(Everest::EV_MQTT_BROKER_HOST);
    const auto mqtt_broker_port = std::getenv(Everest::EV_MQTT_BROKER_PORT);

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
        return Everest::create_mqtt_settings(mqtt_broker_host, mqtt_broker_port_, mqtt_everest_prefix,
                                             mqtt_external_prefix);
    } else {
        return Everest::create_mqtt_settings(mqtt_broker_socket_path, mqtt_everest_prefix, mqtt_external_prefix);
    }
}

RuntimeSession::RuntimeSession(const Everest::MQTTSettings& mqtt_settings, const std::string& logging_config) {
    this->mqtt_settings = mqtt_settings;
    if (logging_config.empty()) {
        this->logging_config_file = Everest::assert_dir(Everest::defaults::PREFIX, "Default prefix") /
                                    std::filesystem::path(Everest::defaults::SYSCONF_DIR) /
                                    Everest::defaults::NAMESPACE / Everest::defaults::LOGGING_CONFIG_NAME;
    } else {
        this->logging_config_file = Everest::assert_file(logging_config, "Default logging config");
    }
}

/// This is just kept for compatibility
RuntimeSession::RuntimeSession(const std::string& prefix, const std::string& config_file) {
    EVLOG_warning
        << "everestpy: Usage of the old RuntimeSession ctor detected, config should be loaded via MQTT not via "
           "the provided config_file. For this please set the appropriate environment variables and call "
           "RuntimeSession()";

    // We extract the settings from the config file so everest-testing doesn't break
    const auto ms = Everest::ManagerSettings(prefix, config_file);

    this->logging_config_file = ms.runtime_settings->logging_config_file;

    this->mqtt_settings = ms.mqtt_settings;
}

RuntimeSession::RuntimeSession() {
    this->logging_config_file =
        Everest::assert_file(get_variable_from_env("EV_LOG_CONF_FILE"), "Default logging config");

    this->mqtt_settings = get_mqtt_settings_from_env();
}

ModuleSetup create_setup_from_config(const std::string& module_id, Everest::Config& config) {
    ModuleSetup setup;

    const std::string& module_name = config.get_module_name(module_id);
    const auto& module_manifest = config.get_manifests().at(module_name);

    // setup connections
    for (const auto& requirement : module_manifest.at("requires").items()) {

        const auto& requirement_id = requirement.key();

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

        for (std::size_t i = 0; i < req_route_list.size(); i++) {
            const auto& req_route = req_route_list[i];
            const auto fulfillment =
                Fulfillment{req_route["module_id"], req_route["implementation_id"], {requirement_id, i}};
            fulfillment_list.emplace_back(fulfillment);
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

        std::size_t errors_size = 0;
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
