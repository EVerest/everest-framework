// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <future>

#include <fmt/core.h>

#include <everest/exceptions.hpp>
#include <everest/logging.hpp>

#include <utils/module_config.hpp>
#include <utils/types.hpp>

namespace Everest {
using json = nlohmann::json;

inline constexpr int mqtt_get_config_timeout_ms = 5000;
using FutureCallback = std::tuple<AsyncReturn, std::function<std::string(json)>>;

void populate_future_cbs(std::vector<FutureCallback>& future_cbs, const std::shared_ptr<MQTTAbstraction>& mqtt,
                         const std::string& everest_prefix, const std::string& topic, json& out) {
    future_cbs.push_back(std::make_tuple<AsyncReturn, std::function<std::string(json)>>(
        mqtt->get_async(topic, QOS::QOS2), [&topic, &everest_prefix, &out](json result) {
            out = result;

            return topic;
        }));
}

void populate_future_cbs_arr(std::vector<FutureCallback>& future_cbs, const std::shared_ptr<MQTTAbstraction>& mqtt,
                             const std::string& everest_prefix, const std::string& topic,
                             const std::string& inner_topic_part, json& array_out, json& out) {
    future_cbs.push_back(std::make_tuple<AsyncReturn, std::function<std::string(json)>>(
        mqtt->get_async(topic, QOS::QOS2),
        [&topic, &everest_prefix, &mqtt, &inner_topic_part, &array_out, &out](json result_array) {
            array_out = result_array;
            std::vector<FutureCallback> array_future_cbs;
            for (const auto& key : result_array) {
                const auto key_topic = fmt::format("{}{}{}", everest_prefix, inner_topic_part, key.get<std::string>());
                array_future_cbs.push_back(std::make_tuple<AsyncReturn, std::function<std::string(json)>>(
                    mqtt->get_async(key_topic, QOS::QOS2), [&key, &key_topic, &out](json key_response) {
                        out[key] = key_response;
                        return key_topic;
                    }));
            }
            for (auto&& [retval, callback] : array_future_cbs) {
                const auto inner_topic = callback(retval.future.get());
                mqtt->unregister_handler(inner_topic, retval.token);
            }
            return topic;
        }));
}

json get_module_config(const std::shared_ptr<MQTTAbstraction>& mqtt, const std::string& module_id) {
    const auto start_time = std::chrono::system_clock::now();
    const auto& everest_prefix = mqtt->get_everest_prefix();

    const auto get_config_topic = fmt::format("{}modules/{}/get_config", everest_prefix, module_id);

    json result;

    std::vector<FutureCallback> future_cbs;

    // config
    auto config = json::object();
    const auto config_topic = fmt::format("{}modules/{}/config", everest_prefix, module_id);
    populate_future_cbs(future_cbs, mqtt, everest_prefix, config_topic, config);

    const json config_publish_data = json::object({{"type", "full"}});
    mqtt->publish(get_config_topic, config_publish_data, QOS::QOS2);

    // interfaces
    const auto interface_names_topic = fmt::format("{}interfaces", everest_prefix);
    auto interface_names = json::object();
    auto interface_definitions = json::object();
    const std::string interface_inner_topic_part = "interface_definitions/";
    populate_future_cbs_arr(future_cbs, mqtt, everest_prefix, interface_names_topic, interface_inner_topic_part,
                            interface_names, interface_definitions);

    // manifests
    const auto module_names_topic = fmt::format("{}module_names", everest_prefix);
    auto module_names_out = json::object();
    auto manifests = json::object();
    const std::string manifests_inner_topic_part = "manifests/";
    populate_future_cbs_arr(future_cbs, mqtt, everest_prefix, module_names_topic, manifests_inner_topic_part,
                            module_names_out, manifests);

    // types
    const auto type_names_topic = fmt::format("{}types", everest_prefix);
    auto type_names = json::object();
    auto type_definitions = json::object();

    // type_definition keys already start with a / so omit it in the topic name
    const std::string type_definitions_inner_topic_part = "type_definitions";
    populate_future_cbs_arr(future_cbs, mqtt, everest_prefix, type_names_topic, type_definitions_inner_topic_part,
                            type_names, type_definitions);

    // module provides
    auto module_provides = json::object();
    const auto module_provides_topic = fmt::format("{}module_provides", everest_prefix);
    populate_future_cbs(future_cbs, mqtt, everest_prefix, module_provides_topic, module_provides);

    // settings
    auto settings = json::object();
    const auto settings_topic = fmt::format("{}settings", everest_prefix);
    populate_future_cbs(future_cbs, mqtt, everest_prefix, settings_topic, settings);

    // schemas
    auto schemas = json::object();
    const auto schemas_topic = fmt::format("{}schemas", everest_prefix);
    populate_future_cbs(future_cbs, mqtt, everest_prefix, schemas_topic, schemas);

    // error_types_map
    auto error_types_map = json::object();
    const auto error_types_map_topic = fmt::format("{}error_types_map", everest_prefix);
    populate_future_cbs(future_cbs, mqtt, everest_prefix, error_types_map_topic, error_types_map);

    // module_config_cache
    auto module_config_cache = json::object();
    const auto module_config_cache_topic = fmt::format("{}module_config_cache", everest_prefix);
    populate_future_cbs(future_cbs, mqtt, everest_prefix, module_config_cache_topic, module_config_cache);

    for (auto&& [retval, callback] : future_cbs) {
        auto topic = callback(retval.future.get());
        mqtt->unregister_handler(topic, retval.token);
    }

    result["mappings"] = config.at("mappings");
    result["module_config"] = config.at("module_config");
    result["module_names"] = module_names_out;
    result["interface_definitions"] = interface_definitions;
    result["manifests"] = manifests;
    result["types"] = type_definitions;
    result["module_provides"] = module_provides;
    result["settings"] = settings;
    result["schemas"] = schemas;
    result["error_map"] = error_types_map;
    result["module_config_cache"] = module_config_cache;

    const auto end_time = std::chrono::system_clock::now();

    EVLOG_info << "get_module_config(): "
               << std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time).count() << "ms";

    return result;
}

} // namespace Everest
