// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <utils/conversions.hpp>

namespace Everest {
namespace conversions {

nlohmann::json typed_json_map_to_config_map(const nlohmann::json& typed_json_config) {
    nlohmann::json config_map;
    for (auto& entry : typed_json_config.items()) {
        config_map[entry.key()] = entry.value().at("value");
    }
    return config_map;
}
} // namespace conversions
} // namespace Everest