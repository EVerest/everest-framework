// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2023 Pionix GmbH and Contributors to EVerest
#ifndef EVERESTPY_MISC_HPP
#define EVERESTPY_MISC_HPP

#include <string>

#include <framework/runtime.hpp>
#include <utils/mqtt_settings.hpp>
#include <utils/types.hpp>

const std::string get_variable_from_env(const std::string& variable);
const std::string get_variable_from_env(const std::string& variable, const std::string& default_value);

class RuntimeSession {
public:
    /// \brief Allows python modules to directly pass \p mqtt_settings as well as a \p logging_config
    RuntimeSession(const Everest::MQTTSettings& mqtt_settings, const std::string& logging_config);

    [[deprecated("Consider switching to the newer RuntimeSession() or RuntimeSession(mqtt_settings, logging_config) "
                 "ctors that receive module configuration via MQTT")]] RuntimeSession(const std::string& prefix,
                                                                                      const std::string& config_file);

    /// \brief Get settings and configuration via MQTT based on certain environment variables
    RuntimeSession();

    const Everest::MQTTSettings& get_mqtt_settings() const {
        return mqtt_settings;
    }

    const std::filesystem::path& get_logging_config_file() const {
        return logging_config_file;
    }

private:
    Everest::MQTTSettings mqtt_settings;
    std::filesystem::path logging_config_file;
};

struct Interface {
    std::vector<std::string> variables;
    std::vector<std::string> commands;
    std::vector<std::string> errors;
};

Interface create_everest_interface_from_definition(const json& def);

struct ModuleSetup {
    struct Configurations {
        std::map<std::string, json> implementations;
        json module;
    };

    Configurations configs;

    std::map<std::string, std::vector<Fulfillment>> connections;
};

ModuleSetup create_setup_from_config(const std::string& module_id, Everest::Config& config);

#endif // EVERESTPY_MISC_HPP
