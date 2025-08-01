// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest
#include "utils/config/storage.hpp"
#include "utils/config/types.hpp"
#include <everest/compile_time_settings.hpp>
#include <everest/database/exceptions.hpp>
#include <everest/database/sqlite/schema_updater.hpp>
#include <everest/logging.hpp>

#include <utils/config/settings.hpp>
#include <utils/config/storage_sqlite.hpp>
#include <utils/date.hpp>

using json = nlohmann::json;
using namespace everest::db;
using namespace everest::db::sqlite;

namespace everest::config {
namespace {
const std::string& default_module_implementation_id() {
    static const std::string DEFAULT_MODULE_IMPLEMENTATION_ID = "!module";
    return DEFAULT_MODULE_IMPLEMENTATION_ID;
}
} // namespace

/// \brief Helper for accessing the column indices of the SETTING table
enum class SettingColumnIndex : int {
    COL_ID = 0,
    COL_PREFIX,
    COL_CONFIG_FILE,
    COL_CONFIGS_DIR,
    COL_SCHEMAS_DIR,
    COL_MODULES_DIR,
    COL_INTERFACES_DIR,
    COL_TYPES_DIR,
    COL_ERRORS_DIR,
    COL_WWW_DIR,
    COL_LOGGING_CONFIG_FILE,
    COL_CONTROLLER_PORT,
    COL_CONTROLLER_RPC_TIMEOUT_MS,
    COL_MQTT_BROKER_SOCKET_PATH,
    COL_MQTT_BROKER_HOST,
    COL_MQTT_BROKER_PORT,
    COL_MQTT_EVEREST_PREFIX,
    COL_MQTT_EXTERNAL_PREFIX,
    COL_TELEMETRY_PREFIX,
    COL_TELEMETRY_ENABLED,
    COL_VALIDATE_SCHEMA,
    COL_RUN_AS_USER,
    COL_FORWARD_EXCEPTIONS,
};

/// \brief Helper for accessing the column indices of the CONFIGURATION table
enum class ConfigurationColumnIndex {
    COL_MODULE_ID = 1,
    COL_PARAMETER_NAME,
    COL_VALUE,
    COL_MUTABILITY_ID,
    COL_DATATYPE_ID,
    COL_UNIT,
    COL_MODULE_IMPLEMENTATION_ID,
};

/// \brief Helper for accessing the column indices of the CONFIGURATION table of a specific MODULE_ID
enum class ConfigurationColumnModuleIdIndex {
    COL_PARAMETER_NAME = 0,
    COL_VALUE,
    COL_MODULE_IMPLEMENTATION_ID,
    COL_MUTABILITY_ID,
    COL_DATATYPE_ID,
    COL_UNIT,
};

namespace {
int to_int(SettingColumnIndex setting_column_index) {
    return static_cast<int>(setting_column_index);
}

int to_int(ConfigurationColumnIndex configuration_column_index) {
    return static_cast<int>(configuration_column_index);
}

int to_int(ConfigurationColumnModuleIdIndex configuration_column_module_id_index) {
    return static_cast<int>(configuration_column_module_id_index);
}
} // namespace

SqliteStorage::SqliteStorage(const fs::path& db_path, const std::filesystem::path& migration_files_path) {
    db = std::make_unique<Connection>(db_path);

    SchemaUpdater updater{db.get()};

    if (!updater.apply_migration_files(migration_files_path, TARGET_MIGRATION_FILE_VERSION)) {
        if (db_path.parent_path().empty()) {
            EVLOG_error
                << "Could not apply migrations for database at provided path: \"" << db_path.string()
                << "\" likely because the database path is just a filename. You MUST provide a full path to the "
                   "database.";
        }
        throw MigrationException("SQL migration failed");
    }

    if (!db->open_connection()) {
        throw std::runtime_error("Could not open database at provided path: " + db_path.string());
    } else {
        EVLOG_info << "Established connection to database successfully: " << db_path;
    }
}

GenericResponseStatus SqliteStorage::write_module_configs(const ModuleConfigurations& module_configs) {
    try {
        auto transaction = this->db->begin_transaction();

        for (const auto& [module_id, module] : module_configs) {
            ModuleData module_data;
            module_data.module_id = module_id;
            module_data.module_name = module.module_name;
            module_data.standalone = module.standalone;
            module_data.capabilities = module.capabilities;

            if (this->write_module_data(module_data) != GenericResponseStatus::OK) {
                EVLOG_error << "Failed to write module info for module: " << module_id;
                return GenericResponseStatus::Failed;
            }

            for (const auto& [requirement_id, connections] : module.connections) {
                for (const auto& connection : connections) {
                    Fulfillment fulfillment;
                    fulfillment.module_id = connection.module_id;
                    fulfillment.implementation_id = connection.implementation_id;
                    fulfillment.requirement = {requirement_id};

                    if (this->write_module_fulfillment(module_id, fulfillment) != GenericResponseStatus::OK) {
                        EVLOG_error << "Failed to write module fulfillment for module: " << module_id
                                    << " and requirement: " << requirement_id;
                        return GenericResponseStatus::Failed;
                    }
                }
            }

            if (module.mapping.module.has_value()) {
                const auto& map = module.mapping.module.value();
                if (this->write_module_tier_mapping(module_id, default_module_implementation_id(), map.evse,
                                                    map.connector) != GenericResponseStatus::OK) {
                    EVLOG_error << "Failed to write module tier mapping for module: " << module_id;
                    return GenericResponseStatus::Failed;
                }
            }

            for (const auto& [impl_id, mapping] : module.mapping.implementations) {
                if (mapping.has_value()) {
                    const auto& map = mapping.value();
                    if (this->write_module_tier_mapping(module_id, impl_id, map.evse, map.connector) !=
                        GenericResponseStatus::OK) {
                        EVLOG_error << "Failed to write module tier mapping for module: " << module_id
                                    << " and implementation id: " << impl_id;
                    }
                }
            }

            for (const auto& [impl_id, params] : module.configuration_parameters) {
                for (const auto& param : params) {
                    ConfigurationParameterIdentifier identifier;
                    identifier.module_id = module_id;
                    identifier.module_implementation_id = impl_id;
                    identifier.configuration_parameter_name = param.name;

                    std::string value;
                    if (std::holds_alternative<std::string>(param.value)) {
                        value = std::get<std::string>(param.value);
                    } else {
                        const nlohmann::json temp = param.value;
                        value = temp.dump();
                    }
                    if (this->write_configuration_parameter(identifier, param.characteristics, value) !=
                        GetSetResponseStatus::OK) {
                        EVLOG_error << "Failed to write configuration parameter for module: " << module_id
                                    << ", param: " << identifier.configuration_parameter_name;
                    }
                }
            }

            if (this->write_access(module_id, module.access) != GenericResponseStatus::OK) {
                EVLOG_error << "Failed to write module access for module: " << module_id;
            }
        }

        transaction->commit();
        return GenericResponseStatus::OK;
    } catch (const std::exception& e) {
        EVLOG_error << "Failed writing config to database: " << e.what();
        return GenericResponseStatus::Failed;
    }
}

GenericResponseStatus SqliteStorage::wipe() {
    const std::string sql = "PRAGMA FOREIGN_KEYS = ON; DELETE FROM MODULE; PRAGMA FOREIGN_KEYS = OFF;";
    try {
        if (this->db->execute_statement(sql)) {
            return GenericResponseStatus::OK;
        }
        return GenericResponseStatus::Failed;
    } catch (const std::exception& e) {
        EVLOG_error << "Failed to wipe database: " << e.what();
        return GenericResponseStatus::Failed;
    }
}

GetModuleConfigsResponse SqliteStorage::get_module_configs() {
    GetModuleConfigsResponse response;

    try {
        const std::string sql = "SELECT ID FROM MODULE";

        auto stmt = this->db->new_statement(sql);

        while (stmt->step() == SQLITE_ROW) {
            auto module_config_response = this->get_module_config(stmt->column_text(0));
            if (module_config_response.status == GenericResponseStatus::OK and
                module_config_response.config.has_value()) {
                response.module_configs[module_config_response.config.value().module_id] =
                    module_config_response.config.value();
            } else {
                response.status = GenericResponseStatus::Failed;
                return response;
            }
        }

        response.status = GenericResponseStatus::OK;
        return response;
    } catch (const std::exception& e) {
        EVLOG_error << "Failed to get EVerest config: " << e.what();
        response.status = GenericResponseStatus::Failed;
        return response;
    }
}

GetSettingsResponse SqliteStorage::get_settings() {
    const std::string sql = "SELECT * FROM SETTING WHERE ID = 0";
    auto stmt = this->db->new_statement(sql);

    if (stmt->step() != SQLITE_ROW) {
        return GetSettingsResponse{GenericResponseStatus::Failed, std::nullopt};
    }

    Settings settings;
    [[maybe_unused]] const auto id =
        stmt->column_int(to_int(SettingColumnIndex::COL_ID)); // ID is required and always present

    // text
    settings.prefix = stmt->column_text(to_int(SettingColumnIndex::COL_PREFIX));
    settings.config_file = stmt->column_text(to_int(SettingColumnIndex::COL_CONFIG_FILE));
    settings.configs_dir = stmt->column_text(to_int(SettingColumnIndex::COL_CONFIGS_DIR));
    settings.schemas_dir = stmt->column_text(to_int(SettingColumnIndex::COL_SCHEMAS_DIR));
    settings.modules_dir = stmt->column_text(to_int(SettingColumnIndex::COL_MODULES_DIR));
    settings.interfaces_dir = stmt->column_text(to_int(SettingColumnIndex::COL_INTERFACES_DIR));
    settings.types_dir = stmt->column_text(to_int(SettingColumnIndex::COL_TYPES_DIR));
    settings.errors_dir = stmt->column_text(to_int(SettingColumnIndex::COL_ERRORS_DIR));
    settings.www_dir = stmt->column_text(to_int(SettingColumnIndex::COL_WWW_DIR));
    settings.logging_config_file = stmt->column_text(to_int(SettingColumnIndex::COL_LOGGING_CONFIG_FILE));
    settings.mqtt_broker_socket_path = stmt->column_text(to_int(SettingColumnIndex::COL_MQTT_BROKER_SOCKET_PATH));
    settings.mqtt_broker_host = stmt->column_text(to_int(SettingColumnIndex::COL_MQTT_BROKER_HOST));
    settings.mqtt_everest_prefix = stmt->column_text(to_int(SettingColumnIndex::COL_MQTT_EVEREST_PREFIX));
    settings.mqtt_external_prefix = stmt->column_text(to_int(SettingColumnIndex::COL_MQTT_EXTERNAL_PREFIX));
    settings.telemetry_prefix = stmt->column_text(to_int(SettingColumnIndex::COL_TELEMETRY_PREFIX));
    settings.run_as_user = stmt->column_text(to_int(SettingColumnIndex::COL_RUN_AS_USER));

    // integer
    settings.controller_port = stmt->column_int(to_int(SettingColumnIndex::COL_CONTROLLER_PORT));
    settings.controller_rpc_timeout_ms = stmt->column_int(to_int(SettingColumnIndex::COL_CONTROLLER_RPC_TIMEOUT_MS));
    settings.mqtt_broker_port = stmt->column_int(to_int(SettingColumnIndex::COL_MQTT_BROKER_PORT));

    // boolean
    settings.telemetry_enabled = stmt->column_int(to_int(SettingColumnIndex::COL_TELEMETRY_ENABLED)) != 0;
    settings.validate_schema = stmt->column_int(to_int(SettingColumnIndex::COL_VALIDATE_SCHEMA)) != 0;
    settings.forward_exceptions = stmt->column_int(to_int(SettingColumnIndex::COL_FORWARD_EXCEPTIONS)) != 0;

    return GetSettingsResponse{GenericResponseStatus::OK, settings};
}

GetModuleConfigurationResponse SqliteStorage::get_module_config(const std::string& module_id) {

    GetModuleConfigurationResponse response;

    const auto module_data_response = this->get_module_data(module_id);
    if (module_data_response.status == GenericResponseStatus::Failed or !module_data_response.module_data.has_value()) {
        response.status = GenericResponseStatus::Failed;
        return response;
    }

    const auto module_data = module_data_response.module_data.value();

    const auto module_fulfillments_response = this->get_module_fulfillments(module_id);
    if (module_fulfillments_response.status == GenericResponseStatus::Failed) {
        response.status = GenericResponseStatus::Failed;
        return response;
    }
    const auto module_fulfillments = module_fulfillments_response.module_fulfillments;

    const auto module_tier_mappings_response = this->get_module_tier_mappings(module_id);
    if (module_tier_mappings_response.status == GenericResponseStatus::Failed) {
        response.status = GenericResponseStatus::Failed;
        return response;
    }
    const auto module_tier_mappings = module_tier_mappings_response.module_tier_mappings;

    const auto config_access_response = this->get_config_access(module_id);
    if (config_access_response.status == GenericResponseStatus::Failed) {
        response.status = GenericResponseStatus::Failed;
        return response;
    }
    const auto config_access = config_access_response.config_access;

    try {

        const std::string sql =
            "SELECT PARAMETER_NAME, VALUE, MODULE_IMPLEMENTATION_ID, MUTABILITY_ID, DATATYPE_ID, UNIT "
            "FROM CONFIGURATION WHERE MODULE_ID = "
            "@module_id";
        auto stmt = this->db->new_statement(sql);

        stmt->bind_text(1, module_id);

        ModuleConfig module_config;
        module_config.capabilities = module_data.capabilities;
        module_config.module_id = module_data.module_id;
        module_config.module_name = module_data.module_name;
        module_config.standalone = module_data.standalone;
        for (const auto& fulfillment : module_fulfillments) {
            module_config.connections[fulfillment.requirement.id].push_back(fulfillment);
        }
        module_config.mapping = module_tier_mappings;
        module_config.access.config = config_access;

        while (stmt->step() == SQLITE_ROW) {
            ConfigurationParameter configuration_parameter;
            configuration_parameter.name =
                stmt->column_text(to_int(ConfigurationColumnModuleIdIndex::COL_PARAMETER_NAME));
            const auto value_str = stmt->column_text(to_int(ConfigurationColumnModuleIdIndex::COL_VALUE));
            auto implementation_id =
                stmt->column_text(to_int(ConfigurationColumnModuleIdIndex::COL_MODULE_IMPLEMENTATION_ID));
            ConfigurationParameterCharacteristics characteristics;
            characteristics.mutability =
                static_cast<Mutability>(stmt->column_int(to_int(ConfigurationColumnModuleIdIndex::COL_MUTABILITY_ID)));
            characteristics.datatype =
                static_cast<Datatype>(stmt->column_int(to_int(ConfigurationColumnModuleIdIndex::COL_DATATYPE_ID)));
            characteristics.unit = stmt->column_text_nullable(to_int(ConfigurationColumnModuleIdIndex::COL_UNIT));
            configuration_parameter.characteristics = characteristics;
            configuration_parameter.value = parse_config_value(characteristics.datatype, value_str);

            module_config.configuration_parameters[implementation_id].push_back(configuration_parameter);
        }
        response.status = GenericResponseStatus::OK;
        response.config = module_config;
        return response;
    } catch (const std::exception& e) {
        EVLOG_error << "Failed to get module config with module_id: " << module_id;
        response.status = GenericResponseStatus::Failed;
        return response;
    }
}

GetConfigurationParameterResponse
SqliteStorage::get_configuration_parameter(const ConfigurationParameterIdentifier& identifier) {

    GetConfigurationParameterResponse response;

    try {
        const std::string sql = "SELECT VALUE, MUTABILITY_ID, DATATYPE_ID, UNIT FROM "
                                "CONFIGURATION WHERE MODULE_ID = "
                                "@module_id AND PARAMETER_NAME = @config_param_name AND MODULE_IMPLEMENTATION_ID = "
                                "@module_implementation_id";
        auto stmt = this->db->new_statement(sql);

        stmt->bind_text("@module_id", identifier.module_id);
        stmt->bind_text("@config_param_name", identifier.configuration_parameter_name);
        stmt->bind_text("@module_implementation_id", identifier.module_implementation_id.has_value()
                                                         ? identifier.module_implementation_id.value()
                                                         : default_module_implementation_id());

        const auto status = stmt->step();

        if (status == SQLITE_DONE) {
            response.status = GetSetResponseStatus::NotFound;
            return response;
        }

        if (status == SQLITE_ROW) {
            response.status = GetSetResponseStatus::OK;

            ConfigurationParameter configuration_parameter;
            configuration_parameter.name = identifier.configuration_parameter_name;
            const auto value_str = stmt->column_text(0);
            ConfigurationParameterCharacteristics characteristics;
            characteristics.mutability = static_cast<Mutability>(stmt->column_int(1));
            characteristics.datatype = static_cast<Datatype>(stmt->column_int(2));
            characteristics.unit = stmt->column_text_nullable(3);
            configuration_parameter.characteristics = characteristics;
            configuration_parameter.value = parse_config_value(characteristics.datatype, value_str);
            response.configuration_parameter = configuration_parameter;
            return response;
        }
    } catch (const std::exception& e) {
        EVLOG_error << "Failed to get config value with module_id: " << identifier.module_id
                    << " and config_parameter_name: " << identifier.configuration_parameter_name;
        response.status = GetSetResponseStatus::Failed;
        return response;
    }

    return response;
}

GetSetResponseStatus
SqliteStorage::write_configuration_parameter(const ConfigurationParameterIdentifier& identifier,
                                             const ConfigurationParameterCharacteristics characteristics,
                                             const std::string& value) {
    try {
        const std::string insert_query = "INSERT OR REPLACE INTO CONFIGURATION (MODULE_ID, PARAMETER_NAME, "
                                         "VALUE, "
                                         "MUTABILITY_ID, DATATYPE_ID, UNIT, MODULE_IMPLEMENTATION_ID) VALUES "
                                         "(?, ?, ?, ?, ?, ?, ?);";

        auto stmt = this->db->new_statement(insert_query);
        stmt->bind_text(to_int(ConfigurationColumnIndex::COL_MODULE_ID), identifier.module_id);
        stmt->bind_text(to_int(ConfigurationColumnIndex::COL_PARAMETER_NAME), identifier.configuration_parameter_name);
        stmt->bind_text(to_int(ConfigurationColumnIndex::COL_VALUE), value);
        stmt->bind_int(to_int(ConfigurationColumnIndex::COL_MUTABILITY_ID),
                       static_cast<int>(characteristics.mutability));
        stmt->bind_int(to_int(ConfigurationColumnIndex::COL_DATATYPE_ID), static_cast<int>(characteristics.datatype));
        if (characteristics.unit.has_value()) {
            stmt->bind_text(to_int(ConfigurationColumnIndex::COL_UNIT), characteristics.unit.value());
        } else {
            stmt->bind_null(to_int(ConfigurationColumnIndex::COL_UNIT));
        }

        if (identifier.module_implementation_id.has_value()) {
            stmt->bind_text(to_int(ConfigurationColumnIndex::COL_MODULE_IMPLEMENTATION_ID),
                            identifier.module_implementation_id.value());
        } else {
            stmt->bind_null(to_int(ConfigurationColumnIndex::COL_MODULE_IMPLEMENTATION_ID));
        }

        if (stmt->step() != SQLITE_DONE) {
            return GetSetResponseStatus::NotFound;
        }

        return GetSetResponseStatus::OK;
    } catch (const std::exception& e) {
        EVLOG_error << "Failed to set config value with module_id: " << identifier.module_id
                    << " and config_parameter_name: " << identifier.configuration_parameter_name;
        return GetSetResponseStatus::Failed;
    }
}

bool SqliteStorage::contains_valid_config() {
    const std::string sql = "SELECT VALID FROM CONFIG_META WHERE ID = 0";
    auto stmt = this->db->new_statement(sql);
    if (stmt->step() != SQLITE_ROW) {
        return false;
    }
    const auto is_valid = stmt->column_int(0);
    return is_valid == 1;
}

void SqliteStorage::mark_valid(const bool is_valid, const std::string& config_dump,
                               const std::optional<fs::path>& config_file_path) {
    const std::string sql =
        "INSERT OR REPLACE INTO CONFIG_META (ID, LAST_UPDATED, VALID, CONFIG_DUMP, CONFIG_FILE_PATH) VALUES (0, "
        "@last_updated, @is_valid, @config_dump, @config_file_path);";
    auto stmt = this->db->new_statement(sql);

    const auto last_updated = Everest::Date::to_rfc3339(date::utc_clock::now());
    stmt->bind_text("@last_updated", last_updated);
    stmt->bind_int("@is_valid", is_valid ? 1 : 0);
    stmt->bind_text("@config_dump", config_dump, SQLiteString::Transient);
    if (config_file_path.has_value()) {
        stmt->bind_text("@config_file_path", config_file_path.value().string(), SQLiteString::Transient);
    } else {
        stmt->bind_null("@config_file_path");
    }
    if (stmt->step() != SQLITE_DONE) {
        EVLOG_error << "Failed to mark config as valid";
    } else {
        EVLOG_debug << "Marked config as valid";
    }
}

GetSetResponseStatus SqliteStorage::update_configuration_parameter(const ConfigurationParameterIdentifier& identifier,
                                                                   const std::string& value) {

    try {
        const std::string update_query = "UPDATE CONFIGURATION SET VALUE = @value "
                                         "WHERE MODULE_ID = @module_id AND "
                                         "PARAMETER_NAME = @parameter_name AND "
                                         "MODULE_IMPLEMENTATION_ID = @module_implementation_id;";

        auto stmt = this->db->new_statement(update_query);
        stmt->bind_text("@value", value);
        stmt->bind_text("@module_id", identifier.module_id);
        stmt->bind_text("@parameter_name", identifier.configuration_parameter_name);
        stmt->bind_text("@module_implementation_id",
                        identifier.module_implementation_id.value_or(default_module_implementation_id()));

        if (stmt->step() != SQLITE_DONE) {
            return GetSetResponseStatus::Failed;
        }

        if (stmt->changes() == 0) {
            return GetSetResponseStatus::NotFound;
        }

        return GetSetResponseStatus::OK;
    } catch (const std::exception& e) {
        EVLOG_error << "Failed to set config value with module_id: " << identifier.module_id
                    << " and config_parameter_name: " << identifier.configuration_parameter_name;
        return GetSetResponseStatus::Failed;
    }
}

GenericResponseStatus SqliteStorage::write_module_data(const ModuleData& module_data) {

    const std::string insert_query =
        "INSERT OR REPLACE INTO MODULE (ID, NAME, STANDALONE, CAPABILITIES) VALUES (?, ?, ?, ?);";

    auto stmt = this->db->new_statement(insert_query);

    stmt->bind_text(1, module_data.module_id);
    stmt->bind_text(2, module_data.module_name);
    stmt->bind_int(3, module_data.standalone);

    if (module_data.capabilities.has_value()) {
        stmt->bind_text(4, module_data.capabilities.value());
    } else {
        stmt->bind_null(4);
    }

    if (stmt->step() != SQLITE_DONE) {
        return GenericResponseStatus::Failed;
    }

    return GenericResponseStatus::OK;
}

GenericResponseStatus SqliteStorage::write_module_fulfillment(const std::string& module_id,
                                                              const Fulfillment& fulfillment) {
    const std::string sql =
        "INSERT OR REPLACE INTO MODULE_FULFILLMENT (MODULE_ID, REQUIREMENT_NAME, IMPLEMENTATION_ID, "
        "IMPLEMENTATION_MODULE_ID) VALUES (?,?,?,?)";

    auto stmt = this->db->new_statement(sql);

    stmt->bind_text(1, module_id);
    stmt->bind_text(2, fulfillment.requirement.id);
    stmt->bind_text(3, fulfillment.implementation_id);
    stmt->bind_text(4, fulfillment.module_id);

    if (stmt->step() != SQLITE_DONE) {
        return GenericResponseStatus::Failed;
    }

    return GenericResponseStatus::OK;
}

GenericResponseStatus SqliteStorage::write_module_tier_mapping(const std::string& module_id,
                                                               const std::string& implementation_id,
                                                               const int32_t evse_id,
                                                               const std::optional<int32_t> connector_id) {
    const std::string sql = "INSERT OR REPLACE INTO MODULE_TIER_MAPPING (MODULE_ID, IMPLEMENTATION_ID, "
                            "EVSE_ID, CONNECTOR_ID) VALUES (?,?,?,?)";

    auto stmt = this->db->new_statement(sql);

    stmt->bind_text(1, module_id);
    stmt->bind_text(2, implementation_id);
    stmt->bind_int(3, evse_id);

    if (connector_id.has_value()) {
        stmt->bind_int(4, connector_id.value());
    } else {
        stmt->bind_null(4);
    }

    if (stmt->step() != SQLITE_DONE) {
        return GenericResponseStatus::Failed;
    }

    return GenericResponseStatus::OK;
}

GenericResponseStatus SqliteStorage::write_access(const std::string& module_id, const Access& access) {
    if (access.config.has_value()) {
        return write_config_access(module_id, access.config.value());
    }
    return GenericResponseStatus::OK;
}
GenericResponseStatus SqliteStorage::write_config_access(const std::string& module_id,
                                                         const ConfigAccess& config_access) {
    // write global config access to db
    const std::string sql = "INSERT OR REPLACE INTO CONFIG_ACCESS (MODULE_ID, ALLOW_GLOBAL_READ, ALLOW_GLOBAL_WRITE, "
                            "ALLOW_SET_READ_ONLY) VALUES (?,?,?,?)";

    auto stmt = this->db->new_statement(sql);

    stmt->bind_text(1, module_id);
    stmt->bind_int(2, config_access.allow_global_read ? 1 : 0);
    stmt->bind_int(3, config_access.allow_global_write ? 1 : 0);
    stmt->bind_int(4, config_access.allow_set_read_only ? 1 : 0);

    if (stmt->step() != SQLITE_DONE) {
        return GenericResponseStatus::Failed;
    }

    // write individual module config access to db
    for (const auto& [other_module_id, module_config_access] : config_access.modules) {
        const auto result = write_module_config_access(module_id, other_module_id, module_config_access);
        if (result != GenericResponseStatus::OK) {
            return GenericResponseStatus::Failed;
        }
    }

    return GenericResponseStatus::OK;
}
GenericResponseStatus SqliteStorage::write_module_config_access(const std::string& module_id,
                                                                const std::string& other_module_id,
                                                                const ModuleConfigAccess& module_config_access) {
    const std::string sql = "INSERT OR REPLACE INTO MODULE_CONFIG_ACCESS (MODULE_ID, OTHER_MODULE_ID, "
                            "ALLOW_READ, ALLOW_WRITE, ALLOW_SET_READ_ONLY) VALUES (?,?,?,?,?)";

    auto stmt = this->db->new_statement(sql);

    stmt->bind_text(1, module_id);
    stmt->bind_text(2, other_module_id);
    stmt->bind_int(3, module_config_access.allow_read ? 1 : 0);
    stmt->bind_int(4, module_config_access.allow_write ? 1 : 0);
    stmt->bind_int(5, module_config_access.allow_set_read_only ? 1 : 0);

    if (stmt->step() != SQLITE_DONE) {
        return GenericResponseStatus::Failed;
    }
    return GenericResponseStatus::OK;
}

GenericResponseStatus SqliteStorage::write_settings(const Everest::ManagerSettings& manager_settings) {
    auto transaction = this->db->begin_transaction();

    std::vector<std::string> keys = {"ID",
                                     "PREFIX",
                                     "CONFIG_FILE",
                                     "CONFIGS_DIR",
                                     "SCHEMAS_DIR",
                                     "MODULES_DIR",
                                     "INTERFACES_DIR",
                                     "TYPES_DIR",
                                     "ERRORS_DIR",
                                     "WWW_DIR",
                                     "LOGGING_CONFIG_FILE",
                                     "CONTROLLER_PORT",
                                     "CONTROLLER_RPC_TIMEOUT_MS",
                                     "MQTT_BROKER_SOCKET_PATH",
                                     "MQTT_BROKER_HOST",
                                     "MQTT_BROKER_PORT",
                                     "MQTT_EVEREST_PREFIX",
                                     "MQTT_EXTERNAL_PREFIX",
                                     "TELEMETRY_PREFIX",
                                     "TELEMETRY_ENABLED",
                                     "VALIDATE_SCHEMA",
                                     "RUN_AS_USER",
                                     "FORWARD_EXCEPTIONS"};

    std::string sql = "INSERT INTO SETTING (";
    for (size_t i = 0; i < keys.size(); ++i) {
        sql += keys.at(i);
        if (i < keys.size() - 1) {
            sql += ", ";
        }
    }
    sql += ") VALUES (";
    for (size_t i = 0; i < keys.size(); ++i) {
        sql += (i == 0 ? "?" : ", ?");
    }
    sql += ") ON CONFLICT(ID) DO UPDATE SET ";
    for (size_t i = 1; i < keys.size(); ++i) {
        sql += keys.at(i) + " = excluded." + keys.at(i);
        if (i < keys.size() - 1) {
            sql += ", ";
        }
    }
    sql += ";";

    auto stmt = this->db->new_statement(sql);

    // ID is always 0
    stmt->bind_int(to_int(SettingColumnIndex::COL_ID) + 1, 0);

    auto bind_text_opt = [&](SettingColumnIndex index, int offset, const std::optional<std::string>& opt) {
        if (opt.has_value()) {
            stmt->bind_text(to_int(index) + offset, opt.value(), SQLiteString::Transient);
        } else {
            stmt->bind_null(to_int(index) + offset);
        }
    };

    auto bind_path_opt = [&](SettingColumnIndex index, int offset, const std::optional<fs::path>& opt) {
        if (opt.has_value()) {
            stmt->bind_text(to_int(index) + offset, opt.value().string(), SQLiteString::Transient);
        } else {
            stmt->bind_null(to_int(index) + offset);
        }
    };

    auto bind_int_opt = [&](SettingColumnIndex index, int offset, const std::optional<int>& opt) {
        if (opt.has_value()) {
            stmt->bind_int(to_int(index) + offset, opt.value());
        } else {
            stmt->bind_null(to_int(index) + offset);
        }
    };

    auto bind_bool_opt = [&](SettingColumnIndex index, int offset, const std::optional<bool>& opt) {
        if (opt) {
            stmt->bind_int(to_int(index) + offset, opt.value() ? 1 : 0);
        } else {
            stmt->bind_null(to_int(index) + offset);
        }
    };

    bind_path_opt(SettingColumnIndex::COL_PREFIX, 1, manager_settings.runtime_settings.prefix);
    bind_path_opt(SettingColumnIndex::COL_CONFIG_FILE, 1, manager_settings.config_file);
    bind_path_opt(SettingColumnIndex::COL_CONFIGS_DIR, 1, manager_settings.configs_dir);
    bind_path_opt(SettingColumnIndex::COL_SCHEMAS_DIR, 1, manager_settings.schemas_dir);
    bind_path_opt(SettingColumnIndex::COL_MODULES_DIR, 1, manager_settings.runtime_settings.modules_dir);
    bind_path_opt(SettingColumnIndex::COL_INTERFACES_DIR, 1, manager_settings.interfaces_dir);
    bind_path_opt(SettingColumnIndex::COL_TYPES_DIR, 1, manager_settings.types_dir);
    bind_path_opt(SettingColumnIndex::COL_ERRORS_DIR, 1, manager_settings.errors_dir);
    bind_path_opt(SettingColumnIndex::COL_WWW_DIR, 1, manager_settings.www_dir);
    bind_path_opt(SettingColumnIndex::COL_LOGGING_CONFIG_FILE, 1,
                  manager_settings.runtime_settings.logging_config_file);

    bind_int_opt(SettingColumnIndex::COL_CONTROLLER_PORT, 1, manager_settings.controller_port);
    bind_int_opt(SettingColumnIndex::COL_CONTROLLER_RPC_TIMEOUT_MS, 1, manager_settings.controller_rpc_timeout_ms);

    bind_text_opt(SettingColumnIndex::COL_MQTT_BROKER_SOCKET_PATH, 1,
                  manager_settings.mqtt_settings.broker_socket_path);
    bind_text_opt(SettingColumnIndex::COL_MQTT_BROKER_HOST, 1, manager_settings.mqtt_settings.broker_host);
    bind_int_opt(SettingColumnIndex::COL_MQTT_BROKER_PORT, 1, manager_settings.mqtt_settings.broker_port);
    bind_text_opt(SettingColumnIndex::COL_MQTT_EVEREST_PREFIX, 1, manager_settings.mqtt_settings.everest_prefix);
    bind_text_opt(SettingColumnIndex::COL_MQTT_EXTERNAL_PREFIX, 1, manager_settings.mqtt_settings.external_prefix);
    bind_text_opt(SettingColumnIndex::COL_TELEMETRY_PREFIX, 1, manager_settings.runtime_settings.telemetry_prefix);

    bind_bool_opt(SettingColumnIndex::COL_TELEMETRY_ENABLED, 1, manager_settings.runtime_settings.telemetry_enabled);
    bind_bool_opt(SettingColumnIndex::COL_VALIDATE_SCHEMA, 1, manager_settings.runtime_settings.validate_schema);
    bind_text_opt(SettingColumnIndex::COL_RUN_AS_USER, 1, manager_settings.run_as_user);
    bind_bool_opt(SettingColumnIndex::COL_FORWARD_EXCEPTIONS, 1, manager_settings.runtime_settings.forward_exceptions);

    if (stmt->step() != SQLITE_DONE) {
        return GenericResponseStatus::Failed;
    }

    transaction->commit();
    return GenericResponseStatus::OK;
}

GetModuleDataResponse SqliteStorage::get_module_data(const std::string& module_id) {

    GetModuleDataResponse response;

    const std::string sql = "SELECT NAME, STANDALONE, CAPABILITIES FROM MODULE WHERE ID = @module_id";

    auto stmt = this->db->new_statement(sql);
    stmt->bind_text("@module_id", module_id);
    const auto status = stmt->step();

    if (status == SQLITE_DONE) {
        response.status = GenericResponseStatus::Failed;
        return response;
    }

    if (status == SQLITE_ROW) {
        ModuleData module_data;
        module_data.module_id = module_id;
        module_data.module_name = stmt->column_text(0);
        module_data.standalone = stmt->column_int(1);
        module_data.capabilities = stmt->column_text_nullable(2);
        response.module_data = module_data;
        response.status = GenericResponseStatus::OK;
    }

    return response;
}

GetModuleFulfillmentsResponse SqliteStorage::get_module_fulfillments(const std::string& module_id) {
    GetModuleFulfillmentsResponse response;

    const std::string sql =
        "SELECT REQUIREMENT_NAME, IMPLEMENTATION_ID, IMPLEMENTATION_MODULE_ID FROM MODULE_FULFILLMENT "
        "WHERE MODULE_ID = @module_id";

    auto stmt = this->db->new_statement(sql);
    stmt->bind_text("@module_id", module_id);

    size_t index = 0;
    while (stmt->step() == SQLITE_ROW) {
        Fulfillment fulfillment;
        fulfillment.requirement = {stmt->column_text(0), index};
        fulfillment.implementation_id = stmt->column_text(1);
        fulfillment.module_id = stmt->column_text(2);
        response.module_fulfillments.push_back(fulfillment);
        index++;
    }

    response.status = GenericResponseStatus::OK; // FIXME: when to return failed?
    return response;
}

GetModuleTierMappingsResponse SqliteStorage::get_module_tier_mappings(const std::string& module_id) {
    GetModuleTierMappingsResponse response;

    const std::string sql = "SELECT IMPLEMENTATION_ID, EVSE_ID, CONNECTOR_ID FROM MODULE_TIER_MAPPING "
                            "WHERE MODULE_ID = @module_id";

    auto stmt = this->db->new_statement(sql);
    stmt->bind_text("@module_id", module_id);

    ModuleTierMappings module_tier_mappings;

    while (stmt->step() == SQLITE_ROW) {
        auto implementation_id = stmt->column_text(0);
        Mapping mapping = {stmt->column_int(1)};
        if (stmt->column_type(2) != SQLITE_NULL) {
            mapping.connector = stmt->column_int(2);
        }
        if (implementation_id == default_module_implementation_id()) {
            module_tier_mappings.module = mapping;
        } else {
            module_tier_mappings.implementations[implementation_id] = mapping;
        }
    }

    response.module_tier_mappings = module_tier_mappings;
    response.status = GenericResponseStatus::OK;
    return response;
}

GetModuleConfigAccessResponse SqliteStorage::get_module_config_access(const std::string& module_id) {
    GetModuleConfigAccessResponse response;
    const std::string sql = "SELECT OTHER_MODULE_ID, ALLOW_SET_READ_ONLY FROM MODULE_CONFIG_ACCESS "
                            "WHERE MODULE_ID = @module_id";

    auto stmt = this->db->new_statement(sql);
    stmt->bind_text("@module_id", module_id);

    std::map<std::string, everest::config::ModuleConfigAccess> module_config_access;
    while (stmt->step() == SQLITE_ROW) {
        const auto other_module_id = stmt->column_text(0);
        const auto allow_set_read_only = stmt->column_int(1) != 0;
        module_config_access[other_module_id].allow_set_read_only = allow_set_read_only;
    }

    response.module_config_access = module_config_access;
    response.status = GenericResponseStatus::OK;
    return response;
}

GetConfigAccessResponse SqliteStorage::get_config_access(const std::string& module_id) {
    GetConfigAccessResponse response;

    const std::string sql =
        "SELECT ALLOW_GLOBAL_READ, ALLOW_SET_READ_ONLY FROM CONFIG_ACCESS WHERE MODULE_ID = @module_id";

    auto stmt = this->db->new_statement(sql);
    stmt->bind_text("@module_id", module_id);

    const auto status = stmt->step();

    if (status == SQLITE_DONE) {
        response.status = GenericResponseStatus::OK;
        return response;
    }

    if (status == SQLITE_ROW) {
        ConfigAccess config_access;
        config_access.allow_global_read = stmt->column_int(0) != 0;
        config_access.allow_set_read_only = stmt->column_int(1) != 0;
        const auto module_config_access = get_module_config_access(module_id);
        if (module_config_access.status == GenericResponseStatus::OK) {
            config_access.modules = module_config_access.module_config_access;
        }
        response.config_access = config_access;
        response.status = GenericResponseStatus::OK;
    }

    return response;
}

} // namespace everest::config
