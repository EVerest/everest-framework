// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#pragma once

#include <utils/config.hpp>
#include <utils/mqtt_abstraction.hpp>
namespace Everest {
namespace config {
enum class Type {
    Get,
    Set,
    Unknown
};

constexpr auto MODULE_IMPLEMENTATION_ID = "!module";

enum class GetType {
    All,    ///< All module configurations that the requesting module has access to
    Module, ///< The module configuration for the requesting module
    Value,  ///< A specific configuration value identified by a ConfigurationParameterIdentifier
    // Delta, // TODO: This needs tracking of when the last Request was made
    AllMappings, ///< All module mappings that the requesting module has access to
    Unknown,
};

struct GetRequest {
    GetType type = GetType::Unknown;
    // TODO: optional timestamp for Delta?
    // TODO: list of requested modules?
    std::optional<everest::config::ConfigurationParameterIdentifier> identifier; ///< Used for GetType::Value
};

struct GetResponse {
    GetType type = GetType::Unknown;
    nlohmann::json data; // FIXME: use proper type(s) for this
};

struct SetRequest {
    everest::config::ConfigurationParameterIdentifier identifier;
    std::string value;
};

enum class ResponseStatus {
    Ok,
    Error,
    AccessDenied
};

enum class SetResponseStatus {
    Accepted,
    Rejected,
    RebootRequired
};

struct SetResponse {
    SetResponseStatus status = SetResponseStatus::Rejected;
};

struct Request {
    Type type = Type::Unknown;
    std::variant<std::monostate, GetRequest, SetRequest> request;
    std::string origin;
};

struct Response {
    ResponseStatus status = ResponseStatus::Error;
    std::optional<Type> type;
    std::variant<std::monostate, GetResponse, SetResponse> response;
};

struct GetConfigResult {
    ResponseStatus status = ResponseStatus::Error;
    everest::config::ConfigurationParameter configuration_parameter;
};

struct ModuleIdType {
    std::string module_id;
    std::string module_type;

    friend bool operator<(const ModuleIdType& lhs, const ModuleIdType& rhs) {
        return (lhs.module_id < rhs.module_id || (lhs.module_id == rhs.module_id && lhs.module_type < rhs.module_type));
    }
};

class ConfigServiceClient {
public:
    ConfigServiceClient(std::shared_ptr<MQTTAbstraction> mqtt_abstraction, const std::string& module_id,
                        const std::unordered_map<std::string, std::string>& module_names);

    std::map<ModuleIdType, everest::config::ModuleConfigurationParameters> get_module_configs();
    std::map<std::string, ModuleTierMappings> get_mappings();

    everest::config::SetConfigStatus
    set_config_value(const everest::config::ConfigurationParameterIdentifier& identifier, const std::string& value);

    GetConfigResult get_config_value(const everest::config::ConfigurationParameterIdentifier& identifier);

private:
    std::shared_ptr<MQTTAbstraction> mqtt_abstraction;
    std::string origin;
    std::unordered_map<std::string, std::string> module_names;
};

class ConfigService {
public:
    ConfigService(MQTTAbstraction& mqtt_abstraction, std::shared_ptr<ManagerConfig> config);

private:
    MQTTAbstraction& mqtt_abstraction;
    std::shared_ptr<TypedHandler> get_config_token;
    std::shared_ptr<ManagerConfig> config;
};

namespace conversions {
std::string type_to_string(Type type);

Type string_to_type(const std::string& type);

std::string get_type_to_string(GetType type);

GetType string_to_get_type(const std::string& type);

std::string response_status_to_string(ResponseStatus status);

ResponseStatus string_to_response_status(const std::string& status);

std::string set_response_status_to_string(SetResponseStatus status);

SetResponseStatus string_to_set_response_status(const std::string& status);

everest::config::SetConfigStatus set_response_status_to_set_config_status(SetResponseStatus status);

SetResponseStatus set_config_status_to_set_response_status(everest::config::SetConfigStatus status);
} // namespace conversions

std::ostream& operator<<(std::ostream& os, const GetType& t);

void to_json(nlohmann::json& j, const GetRequest& r);

void from_json(const nlohmann::json& j, GetRequest& r);

void to_json(nlohmann::json& j, const SetRequest& r);

void from_json(const nlohmann::json& j, SetRequest& r);

void to_json(nlohmann::json& j, const GetResponse& r);

void from_json(const nlohmann::json& j, GetResponse& r);

void to_json(nlohmann::json& j, const SetResponse& r);

void from_json(const nlohmann::json& j, SetResponse& r);

void to_json(nlohmann::json& j, const Request& r);

void from_json(const nlohmann::json& j, Request& r);

void to_json(nlohmann::json& j, const Response& r);

void from_json(const nlohmann::json& j, Response& r);
} // namespace config
} // namespace Everest

NLOHMANN_JSON_NAMESPACE_BEGIN

template <> struct adl_serializer<everest::config::ConfigurationParameterIdentifier> {
    static void to_json(nlohmann::json& j, const everest::config::ConfigurationParameterIdentifier& c);
    static void from_json(const nlohmann::json& j, everest::config::ConfigurationParameterIdentifier& c);
};

NLOHMANN_JSON_NAMESPACE_END
