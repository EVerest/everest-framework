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

json get_module_config(const MQTTSettings& mqtt_settings, const std::string& module_id) {
    auto mqtt = MQTTAbstraction(mqtt_settings);
    if (not mqtt.connect()) {
        EVLOG_error << "Could not connect";
        return 0; // shouldn't this be an error code?
    }
    mqtt.spawn_main_loop_thread();

    auto get_config_topic = fmt::format("{}modules/{}/get_config", mqtt_settings.mqtt_everest_prefix, module_id);
    auto config_topic = fmt::format("{}modules/{}/config", mqtt_settings.mqtt_everest_prefix, module_id);
    std::promise<json> res_promise;
    std::future<json> res_future = res_promise.get_future();

    Handler res_handler = [module_id, &res_promise](json data) {
        EVLOG_verbose << fmt::format("Incoming config for {}", module_id);

        res_promise.set_value(std::move(data));
    };

    std::shared_ptr<TypedHandler> res_token =
        std::make_shared<TypedHandler>(HandlerType::Internal, std::make_shared<Handler>(res_handler));
    mqtt.register_handler(config_topic, res_token, QOS::QOS2);

    json config_publish_data = json::object({{"type", "full"}});

    mqtt.publish(get_config_topic, config_publish_data, QOS::QOS2);

    // wait for result future
    std::chrono::time_point<std::chrono::steady_clock> res_wait =
        std::chrono::steady_clock::now() + std::chrono::seconds(10);
    std::future_status res_future_status;
    do {
        res_future_status = res_future.wait_until(res_wait);
    } while (res_future_status == std::future_status::deferred);

    json result;
    if (res_future_status == std::future_status::timeout) {
        EVLOG_AND_THROW(
            EverestTimeoutError(fmt::format("Timeout while waiting for result of get_config of {}", module_id)));
    } else if (res_future_status == std::future_status::ready) {
        EVLOG_verbose << "res future ready";
        result = res_future.get();
    }
    mqtt.unregister_handler(config_topic, res_token);

    // TODO: disconnect initial mqtt or do we keep this open for additional config updates later on?

    auto interface_names_topic = fmt::format("{}interfaces", mqtt_settings.mqtt_everest_prefix);
    auto interface_names = mqtt.get(interface_names_topic, QOS::QOS2);
    auto interface_definitions = json::object();
    for (auto& interface : interface_names) {
        auto interface_topic =
            fmt::format("{}interface_definitions/{}", mqtt_settings.mqtt_everest_prefix, interface.get<std::string>());
        auto interface_definition = mqtt.get(interface_topic, QOS::QOS2);
        interface_definitions[interface] = interface_definition;
    }

    result["interface_definitions"] = interface_definitions;

    auto types_topic = fmt::format("{}types", mqtt_settings.mqtt_everest_prefix);
    result["types"] = mqtt.get(types_topic, QOS::QOS2);

    auto module_provides_topic = fmt::format("{}module_provides", mqtt_settings.mqtt_everest_prefix);
    auto module_provides = mqtt.get(module_provides_topic, QOS::QOS2);
    result["module_provides"] = module_provides;

    auto settings_topic = fmt::format("{}settings", mqtt_settings.mqtt_everest_prefix);
    auto settings = mqtt.get(settings_topic, QOS::QOS2);
    result["settings"] = settings;

    auto schemas_topic = fmt::format("{}schemas", mqtt_settings.mqtt_everest_prefix);
    auto schemas = mqtt.get(schemas_topic, QOS::QOS2);
    result["schemas"] = schemas;

    auto manifests_topic = fmt::format("{}manifests", mqtt_settings.mqtt_everest_prefix);
    auto manifests = mqtt.get(manifests_topic, QOS::QOS2);
    result["manifests"] = manifests;

    auto error_types_map_topic = fmt::format("{}error_types_map", mqtt_settings.mqtt_everest_prefix);
    auto error_types_map = mqtt.get(error_types_map_topic, QOS::QOS2);
    result["error_map"] = error_types_map;

    auto module_config_cache_topic = fmt::format("{}module_config_cache", mqtt_settings.mqtt_everest_prefix);
    auto module_config_cache = mqtt.get(module_config_cache_topic, QOS::QOS2);
    result["module_config_cache"] = module_config_cache;

    return result;
}

} // namespace Everest
