// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <catch2/catch_all.hpp>

#include <utils/mqtt_abstraction.hpp>
#include <thread>

#include <tests/helpers.hpp>

SCENARIO("MQTT Abstraction", "[!throws]") {
    GIVEN("Valid parameters") {
        const std::string mqtt_server_address = "localhost";
        const std::string mqtt_server_port = "1883";
        const std::string prefix = everest::tests::get_unique_mqtt_test_prefix();
        const std::string mqtt_everest_prefix = prefix + "/everest";
        const std::string mqtt_external_prefix = prefix + "/external";
        THEN("It should connect and not throw on publish") {
            Everest::MQTTAbstraction mqtt_abstraction = Everest::MQTTAbstraction(mqtt_server_address, mqtt_server_port, mqtt_everest_prefix, mqtt_external_prefix);
            CHECK(mqtt_abstraction.connect());
            CHECK_NOTHROW(mqtt_abstraction.spawn_main_loop_thread());
            CHECK_NOTHROW(mqtt_abstraction.publish(mqtt_everest_prefix + "/test", nlohmann::json(true)));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            SUCCEED();
        }
    }
    GIVEN("A connected MQTTAbstraction") {
        const std::string mqtt_server_address = "localhost";
        const std::string mqtt_server_port = "1883";
        const std::string prefix = everest::tests::get_unique_mqtt_test_prefix();
        const std::string mqtt_everest_prefix = prefix + "/everest";
        const std::string mqtt_external_prefix = prefix + "/external";
        Everest::MQTTAbstraction mqtt_abstraction = Everest::MQTTAbstraction(mqtt_server_address, mqtt_server_port, mqtt_everest_prefix, mqtt_external_prefix);
        CHECK(mqtt_abstraction.connect());
        mqtt_abstraction.spawn_main_loop_thread();
        THEN("It should receive message on subscribe") {
            std::promise<nlohmann::json> promise;
            auto handler = [&promise] (const nlohmann::json& message) {
                promise.set_value(message);
            };
            std::future<nlohmann::json> future = promise.get_future();
            const std::string topic = mqtt_everest_prefix + "/test";
            auto token = std::make_shared<TypedHandler>(HandlerType::ExternalMQTT, std::make_shared<Handler>(handler));
            CHECK_NOTHROW(mqtt_abstraction.register_handler(topic, token, QOS::QOS2));
            nlohmann::json message = nlohmann::json(true);
            mqtt_abstraction.publish(topic, message);
            if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
                FAIL("Timeout while waiting for message");
            } else {
                CHECK(future.get() == message);
            }
        }
    }
}
