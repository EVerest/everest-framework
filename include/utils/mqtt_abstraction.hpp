// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef UTILS_MQTT_ABSTRACTION_HPP
#define UTILS_MQTT_ABSTRACTION_HPP

#include <future>

#include <nlohmann/json.hpp>

#include <utils/types.hpp>

namespace Everest {
using json = nlohmann::json;

// forward declaration
class MQTTAbstractionImpl;

/// \brief minimal MQTT connection settings needed for an initial connection of a module to the manager
struct MQTTSettings {
    std::string mqtt_broker_socket_path; ///< A path to a socket the MQTT broker uses in socket mode. If this is set
                                         ///< mqtt_broker_host and mqtt_broker_port are ignored
    std::string mqtt_broker_host;        ///< The hostname of the MQTT broker
    int mqtt_broker_port = 0;            ///< The port the MQTT broker listens on
    std::string mqtt_everest_prefix;     ///< MQTT topic prefix for the "everest" topic
    std::string mqtt_external_prefix;    ///< MQTT topic prefix for external topics
    bool socket = false; ///< Indicates if a Unix Domain Socket is used for connection to the MQTT broker

    /// \brief Creates MQTTSettings with a Unix Domain Socket with the provided \p mqtt_broker_socket_path
    /// using the \p mqtt_everest_prefix and \p mqtt_external_prefix
    MQTTSettings(const std::string& mqtt_broker_socket_path, const std::string& mqtt_everest_prefix,
                 const std::string& mqtt_external_prefix);

    /// \brief Creates MQTTSettings for IP based connections with the provided \p mqtt_broker_host
    /// and \p mqtt_broker_port using the \p mqtt_everest_prefix and \p mqtt_external_prefix
    MQTTSettings(const std::string& mqtt_broker_host, int mqtt_broker_port, const std::string& mqtt_everest_prefix,
                 const std::string& mqtt_external_prefix);
};

///
/// \brief Contains a C++ abstraction for using MQTT in EVerest modules
///
class MQTTAbstraction {
public:
    /// \brief Create a MQTTAbstraction with the provideded \p mqtt_settings
    MQTTAbstraction(const std::shared_ptr<MQTTSettings> mqtt_settings);

    // forbid copy assignment and copy construction
    MQTTAbstraction(MQTTAbstraction const&) = delete;
    void operator=(MQTTAbstraction const&) = delete;

    ~MQTTAbstraction();

    ///
    /// \copydoc MQTTAbstractionImpl::connect()
    bool connect();

    ///
    /// \copydoc MQTTAbstractionImpl::disconnect()
    void disconnect();

    ///
    /// \copydoc MQTTAbstractionImpl::publish(const std::string&, const json&)
    void publish(const std::string& topic, const json& json);

    ///
    /// \copydoc MQTTAbstractionImpl::publish(const std::string&, const json&, QOS)
    void publish(const std::string& topic, const json& json, QOS qos, bool retain = false);

    ///
    /// \copydoc MQTTAbstractionImpl::publish(const std::string&, const std::string&)
    void publish(const std::string& topic, const std::string& data);

    ///
    /// \copydoc MQTTAbstractionImpl::publish(const std::string&, const std::string&, QOS)
    void publish(const std::string& topic, const std::string& data, QOS qos, bool retain = false);

    ///
    /// \copydoc MQTTAbstractionImpl::subscribe(const std::string&)
    void subscribe(const std::string& topic);

    ///
    /// \copydoc MQTTAbstractionImpl::subscribe(const std::string&, QOS)
    void subscribe(const std::string& topic, QOS qos);

    ///
    /// \copydoc MQTTAbstractionImpl::unsubscribe(const std::string&)
    void unsubscribe(const std::string& topic);

    ///
    /// \copydoc MQTTAbstractionImpl::get(const std::string&, QOS)
    json get(const std::string& topic, QOS qos);

    ///
    /// \copydoc MQTTAbstractionImpl::spawn_main_loop_thread()
    std::future<void> spawn_main_loop_thread();

    ///
    /// \copydoc MQTTAbstractionImpl::register_handler(const std::string&, std::shared_ptr<TypedHandler>, QOS)
    void register_handler(const std::string& topic, std::shared_ptr<TypedHandler> handler, QOS qos);

    ///
    /// \copydoc MQTTAbstractionImpl::unregister_handler(const std::string&, const Token&)
    void unregister_handler(const std::string& topic, const Token& token);

private:
    std::unique_ptr<MQTTAbstractionImpl> mqtt_abstraction;
};
} // namespace Everest

#endif // UTILS_MQTT_ABSTRACTION_HPP
