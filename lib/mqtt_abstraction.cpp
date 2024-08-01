// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <everest/logging.hpp>

#include <utils/mqtt_abstraction.hpp>
#include <utils/mqtt_abstraction_impl.hpp>

namespace Everest {
MQTTAbstraction::MQTTAbstraction(const std::string& mqtt_server_socket_path, const std::string& mqtt_server_address,
                                 const std::string& mqtt_server_port, const std::string& mqtt_everest_prefix,
                                 const std::string& mqtt_external_prefix) {
    EVLOG_debug << "initialized mqtt_abstraction";
    if (mqtt_server_socket_path.empty()) {
        mqtt_abstraction = std::make_unique<MQTTAbstractionImpl>(mqtt_server_address, mqtt_server_port,
                                                                 mqtt_everest_prefix, mqtt_external_prefix);
MQTTSettings::MQTTSettings(const std::string& mqtt_broker_socket_path, const std::string& mqtt_everest_prefix,
                           const std::string& mqtt_external_prefix) :
    mqtt_broker_socket_path(mqtt_broker_socket_path),
    mqtt_everest_prefix(mqtt_everest_prefix),
    mqtt_external_prefix(mqtt_external_prefix),
    socket(true) {
}

MQTTSettings::MQTTSettings(const std::string& mqtt_broker_host, int mqtt_broker_port,
                           const std::string& mqtt_everest_prefix, const std::string& mqtt_external_prefix) :
    mqtt_broker_host(mqtt_broker_host),
    mqtt_broker_port(mqtt_broker_port),
    mqtt_everest_prefix(mqtt_everest_prefix),
    mqtt_external_prefix(mqtt_external_prefix),
    socket(false) {
}

    } else {
        mqtt_abstraction =
            std::make_unique<MQTTAbstractionImpl>(mqtt_server_socket_path, mqtt_everest_prefix, mqtt_external_prefix);
    }
}

MQTTAbstraction::~MQTTAbstraction() = default;

bool MQTTAbstraction::connect() {
    BOOST_LOG_FUNCTION();
    return mqtt_abstraction->connect();
}

void MQTTAbstraction::disconnect() {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->disconnect();
}

void MQTTAbstraction::publish(const std::string& topic, const json& json) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->publish(topic, json);
}

void MQTTAbstraction::publish(const std::string& topic, const json& json, QOS qos, bool retain) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->publish(topic, json, qos, retain);
}

void MQTTAbstraction::publish(const std::string& topic, const std::string& data) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->publish(topic, data);
}

void MQTTAbstraction::publish(const std::string& topic, const std::string& data, QOS qos, bool retain) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->publish(topic, data, qos, retain);
}

void MQTTAbstraction::subscribe(const std::string& topic) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->subscribe(topic);
}

void MQTTAbstraction::subscribe(const std::string& topic, QOS qos) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->subscribe(topic, qos);
}

void MQTTAbstraction::unsubscribe(const std::string& topic) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->unsubscribe(topic);
}

json MQTTAbstraction::get(const std::string& topic, QOS qos) {
    BOOST_LOG_FUNCTION();
    return mqtt_abstraction->get(topic, qos);
}

std::future<void> MQTTAbstraction::spawn_main_loop_thread() {
    BOOST_LOG_FUNCTION();
    return mqtt_abstraction->spawn_main_loop_thread();
}

void MQTTAbstraction::register_handler(const std::string& topic, std::shared_ptr<TypedHandler> handler, QOS qos) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->register_handler(topic, handler, qos);
}

void MQTTAbstraction::unregister_handler(const std::string& topic, const Token& token) {
    BOOST_LOG_FUNCTION();
    mqtt_abstraction->unregister_handler(topic, token);
}

} // namespace Everest
