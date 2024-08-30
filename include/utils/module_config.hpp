// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef UTILS_MODULE_CONFIG_HPP
#define UTILS_MODULE_CONFIG_HPP

#include <future>

#include <nlohmann/json.hpp>

#include <utils/mqtt_abstraction.hpp>
#include <utils/types.hpp>

namespace Everest {
using json = nlohmann::json;
namespace fs = std::filesystem;

///
/// \brief Contains helpers for the module config loading
///
class ModuleConfig {
public:
    ModuleConfig() = default;

    /// \brief get config from manager via mqtt
    static json get_config(const MQTTSettings& mqtt_settings, const std::string& module_id);
};
} // namespace Everest

#endif // UTILS_MODULE_CONFIG_HPP
