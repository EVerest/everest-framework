// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <utils/config/settings.hpp>

namespace Everest {

RuntimeSettings::RuntimeSettings(const fs::path& prefix, const fs::path& etc_dir, const fs::path& data_dir,
                                 const fs::path& modules_dir, const fs::path& logging_config_file,
                                 const std::string& telemetry_prefix, bool telemetry_enabled, bool validate_schema) :
    prefix(prefix),
    etc_dir(etc_dir),
    data_dir(data_dir),
    modules_dir(modules_dir),
    logging_config_file(logging_config_file),
    telemetry_prefix(telemetry_prefix),
    telemetry_enabled(telemetry_enabled),
    validate_schema(validate_schema) {
}

RuntimeSettings::RuntimeSettings(const nlohmann::json& json) {
    this->prefix = json.at("prefix").get<std::string>();
    this->etc_dir = json.at("etc_dir").get<std::string>();
    this->data_dir = json.at("data_dir").get<std::string>();
    this->modules_dir = json.at("modules_dir").get<std::string>();
    this->telemetry_prefix = json.at("telemetry_prefix").get<std::string>();
    this->telemetry_enabled = json.at("telemetry_enabled").get<bool>();
    this->validate_schema = json.at("validate_schema").get<bool>();
}

} // namespace Everest

NLOHMANN_JSON_NAMESPACE_BEGIN
void adl_serializer<Everest::RuntimeSettings>::to_json(nlohmann::json& j, const Everest::RuntimeSettings& r) {
    j = {{"prefix", r.prefix},
         {"etc_dir", r.etc_dir},
         {"data_dir", r.data_dir},
         {"modules_dir", r.modules_dir},
         {"telemetry_prefix", r.telemetry_prefix},
         {"telemetry_enabled", r.telemetry_enabled},
         {"validate_schema", r.validate_schema}};
}

void adl_serializer<Everest::RuntimeSettings>::from_json(const nlohmann::json& j, Everest::RuntimeSettings& r) {
    r.prefix = j.at("prefix").get<std::string>();
    r.etc_dir = j.at("etc_dir").get<std::string>();
    r.data_dir = j.at("data_dir").get<std::string>();
    r.modules_dir = j.at("modules_dir").get<std::string>();
    r.telemetry_prefix = j.at("telemetry_prefix").get<std::string>();
    r.telemetry_enabled = j.at("telemetry_enabled").get<bool>();
    r.validate_schema = j.at("validate_schema").get<bool>();
}
NLOHMANN_JSON_NAMESPACE_END
