// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <algorithm>
#include <cstddef>
#include <list>
#include <regex>
#include <set>
#include <tuple>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include <everest/exceptions.hpp>
#include <everest/logging.hpp>

#include <framework/runtime.hpp>
#include <utils/config.hpp>
#include <utils/config/storage.hpp>
#include <utils/config/types.hpp>
#include <utils/formatter.hpp>
#include <utils/yaml_loader.hpp>

using namespace everest::config;

namespace Everest {
using json = nlohmann::json;
using json_uri = nlohmann::json_uri;
using json_validator = nlohmann::json_schema::json_validator;

struct ParsedConfigMap {
    std::vector<ConfigurationParameter> parsed_config_parameters;
    std::set<std::string> unknown_config_entries;
};

void loader(const json_uri& uri, json& schema) {
    BOOST_LOG_FUNCTION();

    if (uri.location() == "http://json-schema.org/draft-07/schema") {
        schema = nlohmann::json_schema::draft7_schema_builtin;
        return;
    }

    // TODO(kai): think about supporting more urls here
    EVTHROW(EverestInternalError(fmt::format("{} is not supported for schema loading at the moment\n", uri.url())));
}

void format_checker(const std::string& format, const std::string& value) {
    BOOST_LOG_FUNCTION();

    if (format == "uri") {
        if (value.find("://") == std::string::npos) {
            EVTHROW(std::invalid_argument("URI does not contain :// - invalid"));
        }
    } else if (format == "uri-reference") {
        /// \brief Allowed format of a type URI, which are of a format like this /type_file_name#/TypeName
        const static std::regex type_uri_regex{R"(^((?:\/[a-zA-Z0-9\-\_]+)+#\/[a-zA-Z0-9\-\_]+)$)"};
        if (!std::regex_match(value, type_uri_regex)) {
            EVTHROW(std::invalid_argument("Type URI is malformed."));
        }
    } else {
        nlohmann::json_schema::default_string_format_check(format, value);
    }
}

std::tuple<nlohmann::json, nlohmann::json_schema::json_validator> load_schema(const fs::path& path) {
    BOOST_LOG_FUNCTION();

    if (!fs::exists(path)) {
        EVLOG_AND_THROW(
            EverestInternalError(fmt::format("Schema file does not exist at: {}", fs::absolute(path).string())));
    }

    EVLOG_debug << fmt::format("Loading schema file at: {}", fs::canonical(path).string());

    json schema = load_yaml(path);

    auto validator = nlohmann::json_schema::json_validator(loader, format_checker);

    try {
        validator.set_root_schema(schema);
    } catch (const std::exception& e) {
        EVLOG_AND_THROW(EverestInternalError(
            fmt::format("Validation of schema '{}' failed, here is why: {}", path.string(), e.what())));
    }

    return std::make_tuple<nlohmann::json, nlohmann::json_schema::json_validator>(std::move(schema),
                                                                                  std::move(validator));
}

SchemaValidation load_schemas(const fs::path& schemas_dir) {
    BOOST_LOG_FUNCTION();
    SchemaValidation schema_validation;

    EVLOG_debug << fmt::format("Loading base schema files for config and manifests... from: {}", schemas_dir.string());
    auto [config_schema, config_val] = load_schema(schemas_dir / "config.yaml");
    schema_validation.schemas.config = config_schema;
    schema_validation.validators.config = std::move(config_val);
    auto [manifest_schema, manifest_val] = load_schema(schemas_dir / "manifest.yaml");
    schema_validation.schemas.manifest = manifest_schema;
    schema_validation.validators.manifest = std::move(manifest_val);
    auto [interface_schema, interface_val] = load_schema(schemas_dir / "interface.yaml");
    schema_validation.schemas.interface = interface_schema;
    schema_validation.validators.interface = std::move(interface_val);
    auto [type_schema, type_val] = load_schema(schemas_dir / "type.yaml");
    schema_validation.schemas.type = type_schema;
    schema_validation.validators.type = std::move(type_val);
    auto [error_declaration_list_schema, error_declaration_list_val] =
        load_schema(schemas_dir / "error-declaration-list.yaml");
    schema_validation.schemas.error_declaration_list = error_declaration_list_schema;
    schema_validation.validators.error_declaration_list = std::move(error_declaration_list_val);

    return schema_validation;
}

json get_serialized_module_config(const std::string& module_id, const ModuleConfigurations& module_configurations) {
    const auto& module_config = module_configurations.at(module_id);
    json serialized_mod_config = json::object();
    serialized_mod_config["module_config"] = module_config; // implicit conversion to json
    serialized_mod_config["mappings"] = json::object();
    for (const auto& [impl_id, fulfillments] : module_config.connections) {
        for (const auto& fulfillment : fulfillments) {
            const auto& mapping = module_configurations.at(fulfillment.module_id).mapping;
            serialized_mod_config["mappings"][fulfillment.module_id] = mapping;
        }
    }
    const auto module_mapping = module_configurations.at(module_id).mapping;
    serialized_mod_config["mappings"][module_id] = module_mapping;
    const auto telemetry_config = module_configurations.at(module_id).telemetry_config;
    if (telemetry_config.has_value()) {
        serialized_mod_config["telemetry_config"] = telemetry_config.value();
    }
    return serialized_mod_config;
}

namespace {
void validate_config_schema(const json& config_map_schema) {
    // iterate over every config entry
    json_validator validator(loader, format_checker);
    for (const auto& config_item : config_map_schema.items()) {
        if (!config_item.value().contains("default")) {
            continue;
        }

        try {
            validator.set_root_schema(config_item.value());
            validator.validate(config_item.value().at("default"));
        } catch (const std::exception& e) {
            throw std::runtime_error(fmt::format("Config item '{}' has issues:\n{}", config_item.key(), e.what()));
        }
    }
}

/// \brief Parses and validates a configuration map against a provided JSON schema.
/// This function processes a list of \p configuration_parameters and validates them
/// against the given \p config_map_schema. It ensures that the types match, default values are applied where
/// necessary, and any unknown configuration entries are detected.
/// \param config_map_schema A JSON object defining the expected configuration structure, including
///        types and default values.
/// \param configuration_parameters A list configuration parameters to be validated
///        against the schema.
/// \return A `ParsedConfigMap` containing:
///         - a list of validated and completed configuration parameters,
///         - a set of unknown configuration keys not present in the schema.
/// \throws ConfigParseException if a required configuration entry is missing, type validation
///         fails against the schema or an unsupported data type is encountered in the schema.
ParsedConfigMap parse_config_map(const json& config_map_schema,
                                 const std::vector<ConfigurationParameter>& configuration_parameters) {
    std::vector<ConfigurationParameter> patched_config_parameters; // this is going to be returned
    std::map<std::string, ConfigurationParameter> config_parameter_map;
    std::set<std::string> config_map_keys;

    for (const auto& param : configuration_parameters) {
        config_parameter_map[param.name] = param;
        config_map_keys.insert(param.name);
    }

    std::set<std::string> unknown_config_entries;
    const std::set<std::string> config_map_schema_keys = Config::keys(config_map_schema);

    std::set_difference(config_map_keys.begin(), config_map_keys.end(), config_map_schema_keys.begin(),
                        config_map_schema_keys.end(),
                        std::inserter(unknown_config_entries, unknown_config_entries.end()));

    // validate each config entry
    for (const auto& config_entry_el : config_map_schema.items()) {
        const std::string& config_entry_name = config_entry_el.key();
        const json& config_entry = config_entry_el.value();

        // only convenience exception, would be catched by schema validation below if not thrown here
        if (!config_entry.contains("default") and
            config_parameter_map.find(config_entry_name) == config_parameter_map.end()) {
            throw ConfigParseException(ConfigParseException::MISSING_ENTRY, config_entry_name);
        }

        json config_entry_value;
        if (config_parameter_map.find(config_entry_name) != config_parameter_map.end()) {
            const auto& expected_datatype = config_parameter_map.at(config_entry_name).characteristics.datatype;
            const auto& actual_datatype = string_to_datatype(config_entry.at("type"));
            if (expected_datatype != actual_datatype) {
                // allow discrepancy when expected datatype is Integer but the actual datatype is Decimal which can
                // present as Integer in the json representation
                if (not(expected_datatype == Datatype::Integer and actual_datatype == Datatype::Decimal)) {
                    throw ConfigParseException(
                        ConfigParseException::SCHEMA, config_entry_name,
                        "Expected and actualy datatypes disagree: " + datatype_to_string(expected_datatype) + " vs " +
                            datatype_to_string(actual_datatype));
                }
            }
            config_parameter_map[config_entry_name].characteristics.datatype = actual_datatype;

            config_entry_value = config_parameter_map[config_entry_name].value; // implicit conversion to json

            if (!config_parameter_map[config_entry_name].validate_type()) {
                throw ConfigParseException(ConfigParseException::SCHEMA, config_entry_name,
                                           "Invalid type for configuration entry");
            }
        } else if (config_entry.contains("default")) {
            config_entry_value = config_entry.at("default"); // use default value defined in manifest
        }
        json_validator validator(loader, format_checker);
        validator.set_root_schema(config_entry);
        try {
            auto patch = validator.validate(config_entry_value);
            if (!patch.is_null()) {
                // extend config entry with default values
                config_entry_value = config_entry_value.patch(patch);
            }
        } catch (const std::exception& err) {
            throw ConfigParseException(ConfigParseException::SCHEMA, config_entry_name, err.what());
        }

        ConfigurationParameter config_param;
        config_param.name = config_entry_name;
        config_param.characteristics.datatype = string_to_datatype(config_entry.at("type"));
        config_param.characteristics.mutability = string_to_mutability(config_entry.at("mutability"));
        // TODO: add unit
        switch (config_param.characteristics.datatype) {
        case Datatype::String:
            config_param.value = config_entry_value.get<std::string>();
            break;
        case Datatype::Decimal:
            config_param.value = config_entry_value.get<double>();
            break;
        case Datatype::Integer:
            config_param.value = config_entry_value.get<int>();
            break;
        case Datatype::Boolean:
            config_param.value = config_entry_value.get<bool>();
            break;
        default:
            throw ConfigParseException(ConfigParseException::SCHEMA, config_entry_name,
                                       "Unsupported datatype in config: " + config_entry.at("type").get<std::string>());
        }
        patched_config_parameters.push_back(config_param);
    }

    return {patched_config_parameters, unknown_config_entries};
}

auto get_provides_for_probe_module(const std::string& probe_module_id, const ModuleConfigurations& module_configs,
                                   const json& manifests) {
    auto provides = json::object();

    for (const auto& [module_id, module_config] : module_configs) {
        if (module_config.module_id == probe_module_id) {
            // do not parse ourself
            continue;
        }

        const auto& connections = module_config.connections;

        for (const auto& [req_id, fulfillments] : connections) {
            const auto module_name = module_config.module_name;
            const auto& module_manifest = manifests.at(module_name);

            // FIXME (aw): in principle we would need to check here again, the listed connections are indeed specified
            // in the modules manifest
            const std::string requirement_interface = module_manifest.at("requires").at(req_id).at("interface");

            for (const auto& fulfillment : fulfillments) {
                const auto impl_mod_id = fulfillment.module_id;
                const auto impl_id = fulfillment.implementation_id;

                if (impl_mod_id != probe_module_id) {
                    continue;
                }

                if (provides.contains(impl_id) && (provides[impl_id].at("interface") != requirement_interface)) {
                    EVLOG_AND_THROW(EverestConfigError(
                        "ProbeModule can not fulfill multiple requirements for the same implementation id '" + impl_id +
                        "', but with different interfaces"));
                } else {
                    provides[impl_id] = {{"interface", requirement_interface}, {"description", "none"}};
                }
            }
        }
    }

    if (provides.empty()) {
        provides["none"] = {{"interface", "empty"}, {"description", "none"}};
    }

    return provides;
}

auto get_requirements_for_probe_module(const std::string& probe_module_id, const ModuleConfigurations& module_configs,
                                       const json& manifests) {
    ModuleConfig probe_module_config;
    for (const auto& [module_id, module_config] : module_configs) {
        if (module_config.module_id == probe_module_id) {
            probe_module_config = module_config;
            break;
        }
    }

    if (probe_module_config.connections.empty()) {
        return json::object();
    }

    auto requirements = json::object();

    for (const auto& [req_id, fulfillments] : probe_module_config.connections) {
        for (const auto& fulfillment : fulfillments) {
            const auto module_id = fulfillment.module_id;
            const auto impl_id = fulfillment.implementation_id;

            if (module_configs.find(module_id) == module_configs.end()) {
                EVLOG_AND_THROW(
                    EverestConfigError(fmt::format("ProbeModule refers to a non-existent module id '{}'", module_id)));
            }

            const auto& module_manifest = manifests.at(module_configs.at(module_id).module_name);

            const auto& module_provides_it = module_manifest.find("provides");

            if (module_provides_it == module_manifest.end()) {
                EVLOG_AND_THROW(EverestConfigError(fmt::format(
                    "ProbeModule requires something from module id '{}' but it does not provide anything", module_id)));
            }

            const auto& provide_it = module_provides_it->find(impl_id);
            if (provide_it == module_provides_it->end()) {
                EVLOG_AND_THROW(EverestConfigError(
                    fmt::format("ProbeModule requires something from module id '{}', but it does not provide '{}'",
                                module_id, impl_id)));
            }

            const std::string interface = provide_it->at("interface");

            if (requirements.contains(req_id) && (requirements[req_id].at("interface") != interface)) {
                // FIXME (aw): we might need to adujst the min/max values here for possible implementations
                EVLOG_AND_THROW(EverestConfigError("ProbeModule interface mismatch -- FIXME (aw)"));
            } else {
                requirements[req_id] = {{"interface", interface}};
            }
        }
    }

    return requirements;
}

void setup_probe_module_manifest(const std::string& probe_module_id, const ModuleConfigurations& module_configs,
                                 json& manifests) {
    // setup basic information
    auto& manifest = manifests["ProbeModule"];
    manifest = {
        {"description", "ProbeModule (generated)"},
        {
            "metadata",
            {
                {"license", "https://opensource.org/licenses/Apache-2.0"},
                {"authors", {"everest"}},
            },
        },
    };

    manifest["provides"] = get_provides_for_probe_module(probe_module_id, module_configs, manifests);

    auto requirements = get_requirements_for_probe_module(probe_module_id, module_configs, manifests);
    if (not requirements.empty()) {
        manifest["requires"] = requirements;
    }
}

ImplementationInfo extract_implementation_info(const std::unordered_map<std::string, std::string>& module_names,
                                               const json& manifests, const std::string& module_id,
                                               const std::string& impl_id) {
    BOOST_LOG_FUNCTION();

    if (module_names.find(module_id) == module_names.end()) {
        EVTHROW(EverestApiError(fmt::format("Module id '{}' not found in config!", module_id)));
    }
    ImplementationInfo info;
    info.module_id = module_id;
    info.module_name = module_names.at(module_id);
    info.impl_id = impl_id;

    if (!impl_id.empty()) {
        if (not manifests.contains(info.module_name)) {
            EVTHROW(EverestApiError(fmt::format("No known manifest for module name '{}'!", info.module_name)));
        }

        if (not manifests.at(info.module_name).at("provides").contains(impl_id)) {
            EVTHROW(EverestApiError(fmt::format("Implementation id '{}' not defined in manifest of module '{}'!",
                                                impl_id, info.module_name)));
        }

        info.impl_intf = manifests.at(info.module_name).at("provides").at(impl_id).at("interface");
    }

    return info;
}

std::string create_printable_identifier(const ImplementationInfo& info, const std::string& /*module_id*/,
                                        const std::string& impl_id) {
    BOOST_LOG_FUNCTION();

    // no implementation id yet so only return this kind of string:
    auto module_string = fmt::format("{}:{}", info.module_id, info.module_name);
    if (impl_id.empty()) {
        return module_string;
    }
    return fmt::format("{}->{}:{}", module_string, info.impl_id, info.impl_intf);
}
} // namespace

// ConfigBase

std::string ConfigBase::printable_identifier(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();

    return printable_identifier(module_id, "");
}

std::string ConfigBase::printable_identifier(const std::string& module_id, const std::string& impl_id) const {
    BOOST_LOG_FUNCTION();

    const auto info = extract_implementation_info(this->module_names, this->manifests, module_id, impl_id);
    return create_printable_identifier(info, module_id, impl_id);
}

std::string ConfigBase::get_module_name(const std::string& module_id) const {
    return this->module_names.at(module_id);
}

std::string ConfigBase::mqtt_prefix(const std::string& module_id, const std::string& impl_id) {
    BOOST_LOG_FUNCTION();

    return fmt::format("{}modules/{}/impl/{}", this->mqtt_settings.everest_prefix, module_id, impl_id);
}

std::string ConfigBase::mqtt_module_prefix(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();

    return fmt::format("{}modules/{}", this->mqtt_settings.everest_prefix, module_id);
}

const ModuleConfigurations& ConfigBase::get_module_configurations() const {
    BOOST_LOG_FUNCTION();
    return this->module_configs;
}

bool ConfigBase::contains(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();
    return this->module_configs.find(module_id) != this->module_configs.end();
}

const json& ConfigBase::get_manifests() const {
    BOOST_LOG_FUNCTION();
    return this->manifests;
}

const json& ConfigBase::get_interface_definitions() const {
    BOOST_LOG_FUNCTION();
    return this->interface_definitions;
}

const json& ConfigBase::get_interfaces() const {
    BOOST_LOG_FUNCTION();
    return this->interfaces;
}

const json& ConfigBase::get_settings() const {
    BOOST_LOG_FUNCTION();
    return this->settings;
}

const json ConfigBase::get_schemas() const {
    BOOST_LOG_FUNCTION();
    return this->schemas;
}

json ConfigBase::get_error_types() {
    BOOST_LOG_FUNCTION();
    return this->error_map.get_error_types();
}

const json& ConfigBase::get_types() const {
    BOOST_LOG_FUNCTION();
    return this->types;
}

std::unordered_map<std::string, std::string> ConfigBase::get_module_names() const {
    return this->module_names;
}

std::vector<Fulfillment> ConfigBase::resolve_requirement(const std::string& module_id,
                                                         const std::string& requirement_id) const {
    BOOST_LOG_FUNCTION();

    // FIXME (aw): this function should throw, if the requirement id
    //             isn't even listed in the module manifest
    // FIXME (aw): the following if doesn't check for the requirement id
    //             at all
    const auto module_name_it = this->module_names.find(module_id);
    if (module_name_it == this->module_names.end()) {
        EVLOG_AND_THROW(EverestApiError(fmt::format("Requested requirement id '{}' of module {} not found in config!",
                                                    requirement_id, printable_identifier(module_id))));
    }

    // check for connections for this requirement
    const auto& module_config = this->module_configs.at(module_id);
    if (module_config.connections.find(requirement_id) == module_config.connections.end()) {
        return {}; // return an empty array if our config does not contain any connections for this
                   // requirement id
    }

    return module_config.connections.at(requirement_id);
}

std::map<Requirement, Fulfillment> ConfigBase::resolve_requirements(const std::string& module_id) const {
    std::map<Requirement, Fulfillment> requirements;

    const auto& module_name = get_module_name(module_id);
    for (const auto& req_id : Config::keys(this->manifests.at(module_name).at("requires"))) {
        const auto& resolved_req = this->resolve_requirement(module_id, req_id);

        size_t index = 0;
        for (const auto& fulfillment : resolved_req) {
            const auto& resolved_module_id = fulfillment.module_id;
            const auto& resolved_impl_id = fulfillment.implementation_id;
            const auto req = Requirement{req_id, index};
            requirements[req] = {resolved_module_id, resolved_impl_id, req};
            index++;
        }
    }

    return requirements;
}

std::list<Requirement> ConfigBase::get_requirements(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();

    std::list<Requirement> res;

    for (const auto& [requirement, fulfillment] : this->resolve_requirements(module_id)) {
        res.push_back(requirement);
    }

    return res;
}

std::map<std::string, std::vector<Fulfillment>> ConfigBase::get_fulfillments(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();

    std::map<std::string, std::vector<Fulfillment>> res;

    for (const auto& [requirement, fulfillment] : this->resolve_requirements(module_id)) {
        res[requirement.id].push_back(fulfillment);
    }

    return res;
}

// ManagerConfig
void ManagerConfig::load_and_validate_manifest(ModuleConfig& module_config) {
    const auto module_id = module_config.module_id;
    const auto module_name = module_config.module_name;
    this->module_names[module_id] = module_name;
    EVLOG_debug << fmt::format("Found module {}, loading and verifying manifest...", printable_identifier(module_id));

    // load and validate module manifest.json
    const fs::path manifest_path = this->ms.runtime_settings.modules_dir / module_name / "manifest.yaml";
    try {

        if (module_name != "ProbeModule") {
            // FIXME (aw): this is implicit logic, because we know, that the ProbeModule manifest had been set up
            // manually already
            EVLOG_debug << fmt::format("Loading module manifest file at: {}", fs::canonical(manifest_path).string());
            this->manifests[module_name] = load_yaml(manifest_path);
        }

        const auto patch = this->validators.manifest.validate(this->manifests[module_name]);
        if (!patch.is_null()) {
            // extend manifest with default values
            this->manifests[module_name] = this->manifests[module_name].patch(patch);
        }
    } catch (const std::exception& e) {
        EVLOG_AND_THROW(EverestConfigError(fmt::format("Failed to load and parse manifest file {}: {}",
                                                       fs::weakly_canonical(manifest_path).string(), e.what())));
    }

    // validate user-defined default values for the config meta-schemas
    try {
        validate_config_schema(this->manifests[module_name]["config"]);
    } catch (const std::exception& e) {
        EVLOG_AND_THROW(EverestConfigError(
            fmt::format("Failed to validate the module configuration meta-schema for module '{}'. Reason:\n{}",
                        module_name, e.what())));
    }

    for (const auto& impl : this->manifests[module_name]["provides"].items()) {
        try {
            validate_config_schema(impl.value().at("config"));
        } catch (const std::exception& e) {
            EVLOG_AND_THROW(
                EverestConfigError(fmt::format("Failed to validate the implementation configuration meta-schema "
                                               "for implementation '{}' in module '{}'. Reason:\n{}",
                                               impl.key(), module_name, e.what())));
        }
    }

    const std::set<std::string> provided_impls = Config::keys(this->manifests[module_name]["provides"]);

    this->interfaces[module_name] = json({});

    for (const auto& impl_id : provided_impls) {
        EVLOG_debug << fmt::format("Loading interface for implementation: {}", impl_id);
        auto intf_name = this->manifests[module_name]["provides"][impl_id]["interface"].get<std::string>();
        this->interfaces[module_name][impl_id] = intf_name;
        resolve_interface(intf_name);
    }

    // check if config only contains impl_ids listed in manifest file
    std::set<std::string> unknown_impls_in_config;
    std::set<std::string> configured_impls;
    for (const auto& [impl_id, config_paramaters] : module_config.configuration_parameters) {
        if (impl_id == "!module") {
            continue;
        }
        configured_impls.insert(impl_id);
    }

    std::set_difference(configured_impls.begin(), configured_impls.end(), provided_impls.begin(), provided_impls.end(),
                        std::inserter(unknown_impls_in_config, unknown_impls_in_config.end()));

    if (!unknown_impls_in_config.empty()) {
        EVLOG_AND_THROW(EverestApiError(
            fmt::format("Implementation id(s)[{}] mentioned in config, but not defined in manifest of module '{}'!",
                        fmt::join(unknown_impls_in_config, " "), module_name)));
    }

    // validate config entries against manifest file
    for (const auto& impl_id : provided_impls) {
        EVLOG_verbose << fmt::format(
            "Validating implementation config of {} against json schemas defined in module mainfest...",
            printable_identifier(module_id, impl_id));

        std::vector<ConfigurationParameter> configuration_parameters;
        if (module_config.configuration_parameters.find(impl_id) != module_config.configuration_parameters.end()) {
            configuration_parameters = module_config.configuration_parameters.at(impl_id);
        }
        const json config_map_schema = this->manifests[module_name]["provides"][impl_id]["config"];

        try {
            const auto parsed_config_map = parse_config_map(config_map_schema, configuration_parameters);
            if (parsed_config_map.unknown_config_entries.size()) {
                for (const auto& unknown_entry : parsed_config_map.unknown_config_entries) {
                    EVLOG_error << fmt::format(
                        "Unknown config entry '{}' of {} of module '{}' ignored, please fix your config file!",
                        unknown_entry, printable_identifier(module_id, impl_id), module_name);
                }
            }
            module_config.configuration_parameters[impl_id] = parsed_config_map.parsed_config_parameters;
        } catch (const ConfigParseException& err) {
            if (err.err_t == ConfigParseException::MISSING_ENTRY) {
                EVLOG_AND_THROW(EverestConfigError(fmt::format("Missing mandatory config entry '{}' in {}!", err.entry,
                                                               printable_identifier(module_id, impl_id))));
            } else if (err.err_t == ConfigParseException::SCHEMA) {
                EVLOG_AND_THROW(
                    EverestConfigError(fmt::format("Schema validation for config entry '{}' failed in {}! Reason:\n{}",
                                                   err.entry, printable_identifier(module_id, impl_id), err.what)));
            } else {
                throw err;
            }
        }
    }

    // validate config for !module
    {
        std::vector<ConfigurationParameter> configuration_parameters;
        if (module_config.configuration_parameters.find("!module") != module_config.configuration_parameters.end()) {
            configuration_parameters = module_config.configuration_parameters.at("!module");
        }
        const json config_map_schema = this->manifests[module_name]["config"];

        try {
            auto parsed_config_map = parse_config_map(config_map_schema, configuration_parameters);
            if (parsed_config_map.unknown_config_entries.size()) {
                for (const auto& unknown_entry : parsed_config_map.unknown_config_entries) {
                    EVLOG_error << fmt::format(
                        "Unknown config entry '{}' of module '{}' ignored, please fix your config file!", unknown_entry,
                        module_config.module_name);
                }
            }
            module_config.configuration_parameters["!module"] = parsed_config_map.parsed_config_parameters;
        } catch (const ConfigParseException& err) {
            if (err.err_t == ConfigParseException::MISSING_ENTRY) {
                EVLOG_AND_THROW(
                    EverestConfigError(fmt::format("Missing mandatory config entry '{}' for module config in module {}",
                                                   err.entry, module_config.module_name)));
            } else if (err.err_t == ConfigParseException::SCHEMA) {
                EVLOG_AND_THROW(EverestConfigError(fmt::format(
                    "Schema validation for config entry '{}' failed for module config in module {}! Reason:\n{}",
                    err.entry, module_config.module_name, err.what)));
            } else {
                throw err;
            }
        }
    }
}

std::tuple<json, int64_t> ManagerConfig::load_and_validate_with_schema(const fs::path& file_path, const json& schema) {
    const json json_to_validate = load_yaml(file_path);
    int64_t validation_ms = 0;

    const auto start_time_validate = std::chrono::system_clock::now();
    json_validator validator(loader, format_checker);
    validator.set_root_schema(schema);
    validator.validate(json_to_validate);
    const auto end_time_validate = std::chrono::system_clock::now();
    EVLOG_debug
        << "YAML validation of " << file_path.string() << " took: "
        << std::chrono::duration_cast<std::chrono::milliseconds>(end_time_validate - start_time_validate).count()
        << "ms";

    validation_ms =
        std::chrono::duration_cast<std::chrono::milliseconds>(end_time_validate - start_time_validate).count();

    return {json_to_validate, validation_ms};
}

json ManagerConfig::resolve_interface(const std::string& intf_name) {
    // load and validate interface.json and mark interface as seen
    const auto intf_definition = load_interface_file(intf_name);

    this->interface_definitions[intf_name] = intf_definition;
    return intf_definition;
}

json ManagerConfig::load_interface_file(const std::string& intf_name) {
    BOOST_LOG_FUNCTION();
    const fs::path intf_path = this->ms.interfaces_dir / (intf_name + ".yaml");
    try {
        EVLOG_debug << fmt::format("Loading interface file at: {}", fs::canonical(intf_path).string());

        json interface_json = load_yaml(intf_path);

        // this subschema can not use allOf with the draft-07 schema because that will cause our validator to
        // add all draft-07 default values which never validate (the {"not": true} default contradicts everything)
        // --> validating against draft-07 will be done in an extra step below
        auto patch = this->validators.interface.validate(interface_json);
        if (!patch.is_null()) {
            // extend config entry with default values
            interface_json = interface_json.patch(patch);
        }
        interface_json = ManagerConfig::replace_error_refs(interface_json);

        // erase "description"
        if (interface_json.contains("description")) {
            interface_json.erase("description");
        }

        // validate every cmd arg/result and var definition against draft-07 schema
        for (auto& var_entry : interface_json["vars"].items()) {
            auto& var_value = var_entry.value();
            // erase "description"
            if (var_value.contains("description")) {
                var_value.erase("description");
            }
            if (var_value.contains("items")) {
                auto& items = var_value.at("items");
                if (items.contains("description")) {
                    items.erase("description");
                }
                if (items.contains("properties")) {
                    for (auto& property : items.at("properties").items()) {
                        auto& property_value = property.value();
                        if (property_value.contains("description")) {
                            property_value.erase("description");
                        }
                    }
                }
            }
            this->draft7_validator->validate(var_value);
        }
        for (auto& cmd_entry : interface_json["cmds"].items()) {
            auto& cmd = interface_json["cmds"][cmd_entry.key()];
            // erase "description"
            if (cmd.contains("description")) {
                cmd.erase("description");
            }
            for (auto& arguments_entry : cmd["arguments"].items()) {
                auto& arg_entry = arguments_entry.value();
                // erase "description"
                if (arg_entry.contains("description")) {
                    arg_entry.erase("description");
                }
                this->draft7_validator->validate(arg_entry);
            }
            auto& result = interface_json["cmds"][cmd_entry.key()]["result"];
            // erase "description"
            if (result.contains("description")) {
                result.erase("description");
            }
            this->draft7_validator->validate(result);
        }

        return interface_json;
    } catch (const std::exception& e) {
        EVLOG_AND_THROW(EverestConfigError(fmt::format("Failed to load and parse interface file {}: {}",
                                                       fs::weakly_canonical(intf_path).string(), e.what())));
    }
}

std::list<json> ManagerConfig::resolve_error_ref(const std::string& reference) {
    BOOST_LOG_FUNCTION();
    const std::string ref_prefix = "/errors/";
    const std::string err_ref = reference.substr(ref_prefix.length());
    const auto result = err_ref.find("#/");
    std::string err_namespace;
    std::string err_name;
    bool is_error_list = false;
    if (result == std::string::npos) {
        err_namespace = err_ref;
        err_name = "";
        is_error_list = true;
    } else {
        err_namespace = err_ref.substr(0, result);
        err_name = err_ref.substr(result + 2);
        is_error_list = false;
    }
    const fs::path path = this->ms.errors_dir / (err_namespace + ".yaml");
    json error_json = load_yaml(path);
    std::list<json> errors;
    if (is_error_list) {
        for (auto& error : error_json.at("errors")) {
            error["namespace"] = err_namespace;
            errors.push_back(error);
        }
    } else {
        for (auto& error : error_json.at("errors")) {
            if (error.at("name") == err_name) {
                error["namespace"] = err_namespace;
                errors.push_back(error);
                break;
            }
        }
    }
    return errors;
}

json ManagerConfig::replace_error_refs(json& interface_json) {
    BOOST_LOG_FUNCTION();
    if (!interface_json.contains("errors")) {
        return interface_json;
    }
    json errors_new = json::object();
    for (auto& error_entry : interface_json.at("errors")) {
        const std::list<json> errors = resolve_error_ref(error_entry.at("reference"));
        for (auto& error : errors) {
            if (!errors_new.contains(error.at("namespace"))) {
                errors_new[error.at("namespace")] = json::object();
            }
            if (errors_new.at(error.at("namespace")).contains(error.at("name"))) {
                EVLOG_AND_THROW(EverestConfigError(fmt::format("Error name '{}' in namespace '{}' already referenced!",
                                                               error.at("name"), error.at("namespace"))));
            }
            errors_new[error.at("namespace")][error.at("name")] = error;
        }
    }
    interface_json["errors"] = errors_new;
    return interface_json;
}

std::tuple<std::string, std::size_t> parse_requirement_id(const std::string& requirement_id) {
    const auto strpos = requirement_id.find(".");
    if (strpos != std::string::npos) {
        return {requirement_id.substr(0, strpos),
                std::stoi(requirement_id.substr(strpos + 1, requirement_id.length()))};
    }
    return {requirement_id, 0};
}

void ManagerConfig::resolve_all_requirements() {
    BOOST_LOG_FUNCTION();

    EVLOG_debug << "Resolving module requirements...";
    // this whole code will not check existence of keys defined by config or
    // manifest metaschemas these have already been checked by schema validation
    for (auto& [module_id, module_config] : this->module_configs) {
        std::set<std::string> module_config_connections_set;
        for (const auto& [req_id, fulfillments] : module_config.connections) {
            module_config_connections_set.insert(req_id);
        }
        std::set<std::string> unknown_requirement_entries;
        const std::set<std::string> manifest_module_requires_set =
            Config::keys(this->manifests[module_config.module_name]["requires"]);

        std::set_difference(module_config_connections_set.begin(), module_config_connections_set.end(),
                            manifest_module_requires_set.begin(), manifest_module_requires_set.end(),
                            std::inserter(unknown_requirement_entries, unknown_requirement_entries.end()));

        if (!unknown_requirement_entries.empty()) {
            EVLOG_AND_THROW(EverestApiError(fmt::format("Configured connection for requirement id(s) [{}] of {} not "
                                                        "defined as requirement in manifest of module '{}'!",
                                                        fmt::join(unknown_requirement_entries, " "),
                                                        printable_identifier(module_id), module_config.module_name)));
        }

        for (auto& element : this->manifests[module_config.module_name]["requires"].items()) {
            const auto& [requirement_id, requirement_index] = parse_requirement_id(element.key());

            const auto& requirement = element.value();

            if (module_config.connections.find(requirement_id) == module_config.connections.end()) {
                if (requirement.at("min_connections") < 1) {
                    EVLOG_debug << fmt::format("Manifest of {} lists OPTIONAL requirement '{}' which could not be "
                                               "fulfilled and will be ignored...",
                                               printable_identifier(module_id), requirement_id);
                    continue; // stop here, there is nothing we can do
                }
                EVLOG_AND_THROW(EverestConfigError(fmt::format(
                    "Requirement '{}' of module {} not fulfilled: requirement id '{}' not listed in connections!",
                    requirement_id, printable_identifier(module_id), requirement_id)));
            }
            const auto& fulfillments = module_config.connections.at(requirement_id);

            // check if min_connections and max_connections are fulfilled
            if (fulfillments.size() < requirement.at("min_connections") ||
                fulfillments.size() > requirement.at("max_connections")) {
                EVLOG_AND_THROW(EverestConfigError(
                    fmt::format("Requirement '{}' of module {} not fulfilled: requirement list does "
                                "not have an entry count between {} and {}!",
                                requirement_id, printable_identifier(module_id), requirement.at("min_connections"),
                                requirement.at("max_connections"))));
            }

            for (const auto& fulfillment : fulfillments) {
                const std::string& connection_module_id = fulfillment.module_id;
                if (this->module_configs.find(connection_module_id) == this->module_configs.end()) {
                    EVLOG_AND_THROW(EverestConfigError(
                        fmt::format("Requirement '{}' of module {} not fulfilled: module id '{}' not loaded in config!",
                                    requirement_id, printable_identifier(module_id), connection_module_id)));
                }

                const auto& connection_module_name = this->module_configs.at(connection_module_id).module_name;
                const auto& [connection_impl_id, connection_impl_index] =
                    parse_requirement_id(fulfillment.implementation_id);
                const auto& connection_manifest = this->manifests[connection_module_name];

                if (!connection_manifest.at("provides").contains(connection_impl_id)) {
                    EVLOG_AND_THROW(EverestConfigError(
                        fmt::format("Requirement '{}' of module {} not fulfilled: required module {} does not provide "
                                    "an implementation for '{}'!",
                                    requirement_id, printable_identifier(module_id),
                                    printable_identifier(fulfillment.module_id), connection_impl_id)));
                }

                // FIXME: copy here so we can safely erase description and config entries
                // FIXME: if we were to copy here this costs us a huge amount of performance during startup
                // FIXME: or does it really? tests are inconclusive right now...
                auto connection_provides = connection_manifest.at("provides").at(connection_impl_id);
                const auto& quantity = connection_provides.at("quantity");
                if (connection_impl_index >= quantity) {
                    EVLOG_AND_THROW(EverestConfigError(
                        fmt::format("Requirement '{}' of module {} not fulfilled: requirement index '{}' is out of "
                                    "bounds of quantity '{}'!",
                                    requirement_id, printable_identifier(module_id), connection_impl_index, quantity)));
                }
                if (connection_provides.contains("config")) {
                    connection_provides.erase("config");
                }
                if (connection_provides.contains("description")) {
                    connection_provides.erase("description");
                }
                const std::string& requirement_interface = requirement.at("interface");

                // check interface requirement
                if (requirement_interface != connection_provides.at("interface")) {
                    EVLOG_AND_THROW(EverestConfigError(fmt::format(
                        "Requirement '{}' of module {} not fulfilled by connection to module {}: required "
                        "interface "
                        "'{}' is not provided by this implementation! Connected implementation provides interface "
                        "'{}'.",
                        requirement_id, printable_identifier(module_id),
                        printable_identifier(fulfillment.module_id, connection_impl_id), requirement_interface,
                        connection_provides.at("interface").get<std::string>())));
                }
                EVLOG_debug << fmt::format("Manifest of {} lists requirement '{}' which will be fulfilled by {}...",
                                           printable_identifier(module_id), requirement_id,
                                           printable_identifier(fulfillment.module_id, fulfillment.implementation_id));
            }
        }
    }
    EVLOG_debug << "All module requirements resolved successfully...";
}

void ManagerConfig::parse(ModuleConfigurations& module_configs) {
    // load type files
    if (this->ms.runtime_settings.validate_schema) {
        int64_t total_time_validation_ms = 0, total_time_parsing_ms = 0;
        for (auto const& types_entry : fs::recursive_directory_iterator(this->ms.types_dir)) {
            const auto start_time = std::chrono::system_clock::now();
            const auto& type_file_path = types_entry.path();
            if (fs::is_regular_file(type_file_path) && type_file_path.extension() == ".yaml") {
                const auto type_path =
                    std::string("/") + fs::relative(type_file_path, this->ms.types_dir).stem().string();

                try {
                    // load and validate type file, store validated result in this->types
                    EVLOG_verbose << fmt::format("Loading type file at: {}", fs::canonical(type_file_path).c_str());

                    const auto [type_json, validate_ms] =
                        load_and_validate_with_schema(type_file_path, this->schemas.type);
                    total_time_validation_ms += validate_ms;

                    this->types[type_path] = type_json.at("types");
                } catch (const std::exception& e) {
                    EVLOG_AND_THROW(EverestConfigError(fmt::format(
                        "Failed to load and parse type file '{}', reason: {}", type_file_path.string(), e.what())));
                }
            }
            const auto end_time = std::chrono::system_clock::now();
            total_time_parsing_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            EVLOG_debug << "Parsing of type " << types_entry.path().string() << " took: "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms";
        }
        EVLOG_info << "- Types loaded in [" << total_time_parsing_ms - total_time_validation_ms << "ms]";
        EVLOG_info << "- Types validated [" << total_time_validation_ms << "ms]";
    }

    // load error files
    if (this->ms.runtime_settings.validate_schema) {
        int64_t total_time_validation_ms = 0, total_time_parsing_ms = 0;
        for (auto const& errors_entry : fs::recursive_directory_iterator(this->ms.errors_dir)) {
            const auto start_time = std::chrono::system_clock::now();
            const auto& error_file_path = errors_entry.path();
            if (fs::is_regular_file(error_file_path) && error_file_path.extension() == ".yaml") {
                try {
                    // load and validate error file
                    EVLOG_verbose << fmt::format("Loading error file at: {}", fs::canonical(error_file_path).c_str());

                    const auto [error_json, validate_ms] =
                        load_and_validate_with_schema(error_file_path, this->schemas.error_declaration_list);
                    total_time_validation_ms += validate_ms;

                } catch (const std::exception& e) {
                    EVLOG_AND_THROW(EverestConfigError(fmt::format(
                        "Failed to load and parse error file '{}', reason: {}", error_file_path.string(), e.what())));
                }
            }
            const auto end_time = std::chrono::system_clock::now();
            total_time_parsing_ms +=
                std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count();
            EVLOG_debug << "Parsing of error " << errors_entry.path().string() << " took: "
                        << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms";
        }
        EVLOG_info << "- Errors loaded in [" << total_time_parsing_ms - total_time_validation_ms << "ms]";
        EVLOG_info << "- Errors validated [" << total_time_validation_ms << "ms]";
    }

    std::optional<std::string> probe_module_id;

    // load manifest files of configured modules
    for (auto& [module_id, module_config] : module_configs) {
        if (module_config.module_name == "ProbeModule") {
            if (probe_module_id) {
                EVLOG_AND_THROW(EverestConfigError("Multiple instance of module type ProbeModule not supported yet"));
            }
            probe_module_id = module_id;
            continue;
        }

        load_and_validate_manifest(module_config);
    }

    if (probe_module_id) {
        auto& probe_module_config = module_configs.at(probe_module_id.value());
        setup_probe_module_manifest(probe_module_config.module_id, module_configs, this->manifests);

        load_and_validate_manifest(probe_module_config);
    }

    for (const auto& [module_id, module_config] : module_configs) {
        this->module_configs[module_id] = module_config;
    }

    resolve_all_requirements();
    parse_3_tier_model_mapping();

    // TODO: cleanup "descriptions" from config ?
}

void ManagerConfig::parse_3_tier_model_mapping() {
    for (const auto& [module_id, module_config] : this->module_configs) {
        const auto& module_name = module_config.module_name;
        const auto& provides = this->manifests.at(module_name).at("provides");

        const auto& config_mapping = module_config.mapping;
        // an empty mapping means it is mapped to the charging station and gets no specific mapping attached

        const auto& implementations_mapping = config_mapping.implementations;
        for (auto& [impl_id, impl_mapping] : implementations_mapping) {
            if (!impl_mapping.has_value()) {
                continue;
            }
            if (!provides.contains(impl_id)) {
                EVLOG_warning << fmt::format("Mapping {} of module {} in config refers to a provides that does "
                                             "not exist, please fix this",
                                             impl_id, printable_identifier(module_id));
            }
        }
    }
}

ManagerConfig::ManagerConfig(const ManagerSettings& ms) : ConfigBase(ms.mqtt_settings), ms(ms) {
    BOOST_LOG_FUNCTION();

    this->manifests = json({});
    this->interfaces = json({});
    this->interface_definitions = json({});
    this->types = json({});
    auto schema_validation = load_schemas(this->ms.schemas_dir);
    this->schemas = schema_validation.schemas;
    this->validators = std::move(schema_validation.validators);
    this->error_map = error::ErrorTypeMap(this->ms.errors_dir);
    this->draft7_validator = std::make_unique<json_validator>(loader, format_checker);
    const static json draft07 = R"(
        {
            "$ref": "http://json-schema.org/draft-07/schema#"
        }
        
        )"_json;
    this->draft7_validator->set_root_schema(draft07);

    ModuleConfigurations module_configs;
    this->settings = this->ms.runtime_settings;
    bool write_config_to_storage = false;
    try {
        if (this->ms.boot_mode == ConfigBootMode::YamlFile) {
            EVLOG_info << "Boot mode is set to YamlFile, loading module configs from YAML file";
            const auto complete_config = this->apply_user_config_and_defaults();
            module_configs = parse_module_configs(complete_config.value("active_modules", json::object()));
        } else if (this->ms.boot_mode == ConfigBootMode::Database) {
            EVLOG_info << "Boot mode is set to Database, loading module configs from database";
            if (this->ms.storage == nullptr) {
                EVLOG_AND_THROW(EverestConfigError("No storage configured, cannot load module configs from database!"));
            }
            if (!this->ms.storage->contains_valid_config()) {
                EVLOG_AND_THROW(EverestConfigError("No valid config found in database"));
            }
            const auto module_configs_response = this->ms.storage->get_module_configs();
            if (module_configs_response.status == GenericResponseStatus::Failed) {
                EVLOG_AND_THROW(EverestConfigError("Failed to load module configs from database"));
            }
            module_configs = module_configs_response.module_configs;
        } else if (this->ms.boot_mode == ConfigBootMode::DatabaseInit) {
            EVLOG_info << "Boot mode is set to DatabaseInit";
            if (this->ms.storage == nullptr) {
                EVLOG_AND_THROW(EverestConfigError("No storage configured, cannot load module configs from database!"));
            }
            if (this->ms.storage->contains_valid_config()) {
                EVLOG_info << "Storage contains valid config, loading module configs from database";
                const auto module_configs_response = this->ms.storage->get_module_configs();
                if (module_configs_response.status == GenericResponseStatus::Failed) {
                    EVLOG_AND_THROW(EverestConfigError("Failed to load module configs from database"));
                } else {
                    module_configs = module_configs_response.module_configs;
                }
            } else {
                EVLOG_info << "Storage does not contain valid config, "
                              "loading module configs from YAML file as fallback";
                this->ms.storage->wipe();       // make sure we write a fresh config
                write_config_to_storage = true; // we can only write the config to the storage after the parse()
                                                // function, since this adds meta data like characteristics to the
                                                // module_configs that is required for writing to the storage
                // fallback to loading from YAML file
                const auto complete_config = this->apply_user_config_and_defaults();
                module_configs = parse_module_configs(complete_config.value("active_modules", json::object()));
            }
        }

        this->parse(module_configs);
        // now the config is parsed, validated and patched!

        if (!write_config_to_storage) {
            return;
        }

        if (this->ms.storage->write_module_configs(module_configs) != GenericResponseStatus::Failed) {
            EVLOG_info << "Module configs written to database successfully, marking config as valid";
            this->ms.storage->mark_valid(true, json(module_configs).dump(), this->ms.config_file);
        } else {
            EVLOG_warning << "Failed to write module configs to database, marking config as invalid";
            this->ms.storage->mark_valid(false, json(module_configs).dump(), this->ms.config_file);
        }
    } catch (const std::exception& e) {
        EVLOG_AND_THROW(EverestConfigError(fmt::format("Failed to load and parse configuration: {}", e.what())));
    }
}

json ManagerConfig::apply_user_config_and_defaults() {
    // load and process config file
    const fs::path config_path = this->ms.config_file;
    EVLOG_info << fmt::format("Loading config file at: {}", fs::canonical(config_path).string());
    // this config is parsed from the file, it doesnt contain any defaults or patches!
    auto complete_config = this->ms.config;
    // try to load user config from a directory "user-config" that might be in the same parent directory as the
    // config_file. The config is supposed to have the same name as the parent config.
    // TODO(kai): introduce a parameter that can overwrite the location of the user config?
    // TODO(kai): or should we introduce a "meta-config" that references all configs that should be merged here?
    const auto user_config_path = config_path.parent_path() / "user-config" / config_path.filename();
    this->user_config_storage = std::make_unique<everest::config::UserConfigStorage>(user_config_path);
    if (fs::exists(user_config_path)) {
        EVLOG_info << fmt::format("Loading user-config file at: {}", fs::canonical(user_config_path).string());
        EVLOG_debug << "Augmenting main config with user-config entries";
        complete_config.merge_patch(this->user_config_storage->get_user_config());
    } else {
        EVLOG_verbose << "No user-config provided.";
    }

    const auto patch = this->validators.config.validate(complete_config);
    if (!patch.is_null()) {
        // extend config with default values
        complete_config = complete_config.patch(patch);
    }
    return complete_config;
}

// Config

Config::Config(const MQTTSettings& mqtt_settings, const json& serialized_config) : ConfigBase(mqtt_settings) {
    this->module_config = serialized_config.at("module_config"); // implicit conversion from JSON
    this->module_configs[this->module_config.module_id] = this->module_config;
    this->manifests = serialized_config.value("manifests", json({}));
    this->interface_definitions = serialized_config.value("interface_definitions", json({}));
    this->types = serialized_config.value("types", json({}));
    this->module_names = serialized_config.at("module_names");

    this->populate_module_config_cache();

    if (serialized_config.contains("mappings") and !serialized_config.at("mappings").is_null()) {
        auto mapping_json = serialized_config.at("mappings");
        for (auto mapping = mapping_json.begin(); mapping != mapping_json.end(); ++mapping) {
            const auto& mapping_name = mapping.key();
            const auto& mapping_value = mapping.value();
            if (!mapping_value.is_null()) {
                this->tier_mappings.emplace(mapping_name, mapping_value.get<ModuleTierMappings>());
            }
        }
    }
    if (serialized_config.contains("telemetry_config") and !serialized_config.at("telemetry_config").is_null()) {
        this->telemetry_config = serialized_config.at("telemetry_config");
    }

    if (serialized_config.contains("schemas")) {
        this->schemas = serialized_config.at("schemas");
    }

    // create error type map from interface definitions
    this->populate_error_map();
}

namespace {
everest::config::ConfigurationParameterCharacteristics
get_characteristics(const std::string& name,
                    const std::vector<everest::config::ConfigurationParameter>& configuration_parameters) {
    for (const auto& configuration_parameter : configuration_parameters) {
        if (configuration_parameter.name == name) {
            return configuration_parameter.characteristics;
        }
    }
    throw std::out_of_range("oops");
}
} // namespace

everest::config::SetConfigStatus
ManagerConfig::set_config_value(const everest::config::ConfigurationParameterIdentifier& identifier,
                                const everest::config::ConfigEntry& value) {
    try {
        const auto& module_config = this->module_configs.at(identifier.module_id);
        const auto& configuration_parameters =
            module_config.configuration_parameters.at(identifier.module_implementation_id.value_or("!module"));
        const auto& characteristics =
            get_characteristics(identifier.configuration_parameter_name, configuration_parameters);

        switch (this->ms.boot_mode) {
        case ConfigBootMode::YamlFile: {
            const auto write_response = this->user_config_storage->write_configuration_parameter(
                identifier, characteristics, everest::config::config_entry_to_string(value));
            if (write_response == GetSetResponseStatus::OK) {
                return everest::config::SetConfigStatus::RebootRequired;
            }
            break;
        }
        case ConfigBootMode::Database:
        case ConfigBootMode::DatabaseInit:
            const auto& cached_value_it = this->database_get_config_parameter_response_cache.find(identifier);
            const auto cached_value = this->ms.storage->get_configuration_parameter(identifier);
            const auto write_response = this->ms.storage->write_configuration_parameter(
                identifier, characteristics, everest::config::config_entry_to_string(value));
            if (write_response == GetSetResponseStatus::OK) {
                if (cached_value_it == this->database_get_config_parameter_response_cache.end()) {
                    // cache initial config value in case it is only valid after a reboot
                    this->database_get_config_parameter_response_cache[identifier] = cached_value;
                }
                return everest::config::SetConfigStatus::RebootRequired;
            }
            return everest::config::SetConfigStatus::Rejected;
        }
    } catch (const std::exception& e) {
        return everest::config::SetConfigStatus::Rejected;
    }

    return everest::config::SetConfigStatus::Rejected;
}

everest::config::GetConfigurationParameterResponse
ManagerConfig::get_config_value(const everest::config::ConfigurationParameterIdentifier& identifier) {
    everest::config::GetConfigurationParameterResponse response;
    response.status = GetSetResponseStatus::Failed;

    try {
        switch (this->ms.boot_mode) {
        case ConfigBootMode::YamlFile: {
            const auto& module_config = this->module_configs.at(identifier.module_id);
            const auto& configuration_parameters =
                module_config.configuration_parameters.at(identifier.module_implementation_id.value_or("!module"));
            for (const auto& configuration_parameter : configuration_parameters) {
                if (configuration_parameter.name == identifier.configuration_parameter_name) {
                    response.status = GetSetResponseStatus::OK;
                    response.configuration_parameter = configuration_parameter;
                    break;
                }
            }
            if (response.status != GetSetResponseStatus::OK) {
                response.status = GetSetResponseStatus::NotFound;
            }
            break;
        }
        case ConfigBootMode::Database:
        case ConfigBootMode::DatabaseInit: {
            // ensure that we do not return database values that are only valid after a reboot
            const auto& cached_value_it = this->database_get_config_parameter_response_cache.find(identifier);
            if (cached_value_it != this->database_get_config_parameter_response_cache.end()) {
                return cached_value_it->second;
            }
            response = this->ms.storage->get_configuration_parameter(identifier);
            break;
        }
        }
    } catch (const std::exception& e) {
        everest::config::GetConfigurationParameterResponse failed_response;
        failed_response.status = GetSetResponseStatus::Failed;
        return failed_response;
    }

    return response;
}

error::ErrorTypeMap Config::get_error_map() const {
    return this->error_map;
}

bool Config::module_provides(const std::string& module_name, const std::string& impl_id) {
    const auto& provides = this->module_config_cache.at(module_name).provides_impl;
    return (provides.find(impl_id) != provides.end());
}

const json& Config::get_module_cmds(const std::string& module_name, const std::string& impl_id) {
    return this->module_config_cache.at(module_name).cmds.at(impl_id);
}

RequirementInitialization Config::get_requirement_initialization(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();

    RequirementInitialization res;

    for (const auto& [requirement, fulfillment] : this->resolve_requirements(module_id)) {
        const auto& mapping = this->get_3_tier_model_mapping(fulfillment.module_id, fulfillment.implementation_id);
        res[requirement.id].push_back({requirement, fulfillment, mapping});
    }

    return res;
}

ModuleConfigs Config::get_module_configs(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();
    ModuleConfigs module_configs;

    // FIXME (aw): throw exception if module_id does not exist
    if (contains(module_id)) {
        for (const auto& [impl_id, config_parameters] : this->module_config.configuration_parameters) {
            ConfigMap processed_conf_map;
            for (const auto& config_parameter : config_parameters) {
                processed_conf_map[config_parameter.name] = config_parameter.value;
            }
            module_configs[impl_id] = processed_conf_map;
        }
    }

    return module_configs;
}

ModuleConfig Config::get_module_config() const {
    BOOST_LOG_FUNCTION();
    return this->module_config;
}

std::optional<ModuleTierMappings> Config::get_module_3_tier_model_mappings(const std::string& module_id) const {
    if (this->tier_mappings.find(module_id) == this->tier_mappings.end()) {
        return std::nullopt;
    }
    return this->tier_mappings.at(module_id);
}

std::optional<Mapping> Config::get_3_tier_model_mapping(const std::string& module_id,
                                                        const std::string& impl_id) const {
    const auto module_tier_mappings = this->get_module_3_tier_model_mappings(module_id);
    if (not module_tier_mappings.has_value()) {
        return std::nullopt;
    }
    const auto& mapping = module_tier_mappings.value();
    if (mapping.implementations.find(impl_id) == mapping.implementations.end()) {
        // if no specific implementation mapping is given, use the module mapping
        return mapping.module;
    }
    return mapping.implementations.at(impl_id);
}

ModuleInfo Config::get_module_info(const std::string& module_id) const {
    BOOST_LOG_FUNCTION();

    ModuleInfo module_info;
    module_info.id = module_id;
    module_info.name = this->module_config.module_name;
    module_info.global_errors_enabled = this->manifests.at(module_info.name).at("enable_global_errors");
    const auto& module_metadata = this->manifests.at(module_info.name).at("metadata");
    for (auto& author : module_metadata.at("authors")) {
        module_info.authors.emplace_back(author.get<std::string>());
    }
    module_info.license = module_metadata.at("license").get<std::string>();

    return module_info;
}

std::optional<TelemetryConfig> Config::get_telemetry_config() {
    return this->module_config.telemetry_config;
}

json Config::get_interface_definition(const std::string& interface_name) const {
    BOOST_LOG_FUNCTION();
    return this->interface_definitions.value(interface_name, json());
}

void Config::populate_module_config_cache() {
    for (const auto& [module_id, module_name] : this->module_names) {
        this->module_config_cache[module_name] = ConfigCache();
        const std::set<std::string> provided_impls = Config::keys(this->manifests.at(module_name).at("provides"));
        this->interfaces[module_name] = json({});
        this->module_config_cache[module_name].provides_impl = provided_impls;
        for (const auto& impl_id : provided_impls) {
            auto intf_name =
                this->manifests.at(module_name).at("provides").at(impl_id).at("interface").get<std::string>();
            this->interfaces[module_name][impl_id] = intf_name;
            this->module_config_cache[module_name].cmds[impl_id] = this->interface_definitions.at(intf_name).at("cmds");
        }
    }
}

void Config::populate_error_map() {
    // TODO(kai): distribute the error information centrally again? (split over multiple
    // topics) since there can be some redundancies with eg. generic errors that might be in multiple interfaces
    // then remove the "errors" entry from the interface definitions that are shared via MQTT, this could reduce their
    // size a bit since it limits the amount of shared redundant information
    json error_types_map = json({});
    for (const auto& [interface_name, interface_definition] : this->interface_definitions.items()) {
        for (const auto& [error_namespace, errors] : interface_definition.at("errors").items()) {
            for (const auto& [error_key, error_definition] : errors.items()) {
                const auto error_type_name = fmt::format("{}/{}", error_definition.at("namespace").get<std::string>(),
                                                         error_definition.at("name").get<std::string>());
                if (not error_types_map.contains(error_type_name)) {
                    error_types_map[error_type_name] = error_definition.at("description").get<std::string>();
                }
            }
        }
    }
    this->error_map = error::ErrorTypeMap();
    this->error_map.load_error_types_map(error_types_map);
}

void Config::ref_loader(const json_uri& uri, json& schema) {
    BOOST_LOG_FUNCTION();

    if (uri.location() == "http://json-schema.org/draft-07/schema") {
        schema = nlohmann::json_schema::draft7_schema_builtin;
        return;
    } else {
        const auto& path = uri.path();
        if (this->types.contains(path)) {
            schema = this->types[path];
            EVLOG_verbose << fmt::format("ref path \"{}\" schema has been found.", path);
            return;
        } else {
            EVLOG_verbose << fmt::format("ref path \"{}\" schema has not been found.", path);
        }
    }

    // TODO(kai): think about supporting more urls here
    EVTHROW(EverestInternalError(fmt::format("{} is not supported for schema loading at the moment\n", uri.url())));
}

json Config::load_all_manifests(const std::string& modules_dir, const std::string& schemas_dir) {
    BOOST_LOG_FUNCTION();

    json manifests = json({});

    auto schema_validation = load_schemas(schemas_dir);

    const fs::path modules_path = fs::path(modules_dir);

    for (auto&& module_path : fs::directory_iterator(modules_path)) {
        const fs::path manifest_path = module_path.path() / "manifest.yaml";
        if (!fs::exists(manifest_path)) {
            continue;
        }

        const std::string module_name = module_path.path().filename().string();
        EVLOG_debug << fmt::format("Found module {}, loading and verifying manifest...", module_name);

        try {
            manifests[module_name] = load_yaml(manifest_path);

            schema_validation.validators.manifest.validate(manifests.at(module_name));
        } catch (const std::exception& e) {
            EVLOG_AND_THROW(EverestConfigError(
                fmt::format("Failed to load and parse module manifest file of module {}: {}", module_name, e.what())));
        }
    }

    return manifests;
}

std::set<std::string> Config::keys(const json& object) {
    BOOST_LOG_FUNCTION();

    std::set<std::string> keys;
    if (!object.is_object()) {
        if (object.is_null() || object.empty()) {
            // if the object is null we should return an empty set
            return keys;
        }
        EVLOG_AND_THROW(
            EverestInternalError(fmt::format("Provided value is not an object. It is a: {}", object.type_name())));
    }

    for (const auto& element : object.items()) {
        keys.insert(element.key());
    }

    return keys;
}

} // namespace Everest

NLOHMANN_JSON_NAMESPACE_BEGIN
void adl_serializer<Everest::Schemas>::to_json(nlohmann::json& j, const Everest::Schemas& s) {
    j = {{"config", s.config},
         {"manifest", s.manifest},
         {"interface", s.interface},
         {"type", s.type},
         {"error_declaration_list", s.error_declaration_list}};
}

void adl_serializer<Everest::Schemas>::from_json(const nlohmann::json& j, Everest::Schemas& s) {
    s.config = j.at("config");
    s.manifest = j.at("manifest");
    s.interface = j.at("interface");
    s.type = j.at("type");
    s.error_declaration_list = j.at("error_declaration_list");
}
NLOHMANN_JSON_NAMESPACE_END
