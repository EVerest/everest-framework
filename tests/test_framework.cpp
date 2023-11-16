// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <catch2/catch_all.hpp>

#include <framework/everest.hpp>
#include <framework/runtime.hpp>
#include <utils/mqtt_abstraction.hpp>

#include <tests/helpers.hpp>

void send_global_ready_signal() {
    Everest::MQTTAbstraction mqtt_abstraction("localhost", "1883", "everest-test-framework-const/", "external/");    
    if (!mqtt_abstraction.connect()) {
        FAIL("Cannot connect to MQTT broker");
    }
    mqtt_abstraction.spawn_main_loop_thread();
    std::this_thread::sleep_for(std::chrono::seconds(1));
    mqtt_abstraction.publish("everest-test-framework-const/ready", nlohmann::json(true));
}

SCENARIO("Check Everest Constructor", "[!throws]") {
    GIVEN("Valid Parameters") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("framework_const/","framework_const/config.yaml"));

        const std::string module_id = "test_module_a";
        Everest::Config config = Everest::Config(rs);
        const bool validate_data_with_schema = false;
        const std::string mqtt_server_address = "localhost";
        const int mqtt_server_port = 1883;
        const std::string prefix = everest::tests::get_unique_mqtt_test_prefix();
        const std::string mqtt_everest_prefix = prefix + "/everest";
        const std::string mqtt_external_prefix = prefix + "/external";
        const std::string telemetry_prefix = prefix + "/telemetry";
        const bool telemetry_enabled = false;

        THEN("It should not throw") {
            CHECK_NOTHROW(Everest::Everest(
                module_id,
                config,
                validate_data_with_schema,
                mqtt_server_address,
                mqtt_server_port,
                mqtt_everest_prefix,
                mqtt_external_prefix,
                telemetry_prefix,
                telemetry_enabled
            ));
        }
    }
}

SCENARIO("Check Everest publish var func", "[!throws]") {
    GIVEN("Initialized Everest instance and MQTTAbstraction") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("framework_const/","framework_const/config.yaml"));

        const std::string module_id = "test_module_a";
        Everest::Config config = Everest::Config(rs);
        const bool validate_data_with_schema = false;
        const std::string mqtt_server_address = "localhost";
        const int mqtt_server_port = 1883;
        const std::string prefix = everest::tests::get_unique_mqtt_test_prefix();
        const std::string mqtt_everest_prefix = prefix + "/everest";
        const std::string mqtt_external_prefix = prefix + "/external";
        const std::string telemetry_prefix = prefix + "/telemetry";
        const bool telemetry_enabled = false;

        Everest::Everest everest(
            module_id,
            config,
            validate_data_with_schema,
            mqtt_server_address,
            mqtt_server_port,
            mqtt_everest_prefix,
            mqtt_external_prefix,
            telemetry_prefix,
            telemetry_enabled
        );
        if(!everest.connect()) {
            FAIL("Cannot connect to MQTT broker");
        }
        everest.spawn_main_loop_thread();

        Everest::MQTTAbstraction mqtt_abstraction(
            mqtt_server_address,
            std::to_string(mqtt_server_port),
            mqtt_everest_prefix,
            mqtt_external_prefix
        );
        if(!mqtt_abstraction.connect()) {
            FAIL("Cannot connect to MQTT broker");
        }
        mqtt_abstraction.spawn_main_loop_thread();

        WHEN("Didn't received ready signal") {
            THEN("It shouldn't work") {
                std::string impl_id = "main";
                std::string var_name = "test_var";
                std::string test_value = "test_value";

                std::promise<nlohmann::json> promise;
                auto handler = [&promise](nlohmann::json data) {
                    promise.set_value(data);
                };
                auto token = std::make_shared<TypedHandler>(HandlerType::SubscribeVar, std::make_shared<Handler>(handler));
                const std::string mqtt_var_topic = mqtt_everest_prefix + "/" + module_id + "/" + impl_id + "/var";
                mqtt_abstraction.register_handler(mqtt_var_topic, token, QOS::QOS2);
                everest.publish_var(
                    impl_id,
                    var_name,
                    test_value
                );
                std::future<nlohmann::json> future = promise.get_future();
                if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
                    SUCCEED("Didn't receive var");
                } else {
                    FAIL("Shouldn't receive var, but received: " + future.get().dump());
                }
            }
        }
        WHEN("Received ready signal") {
            mqtt_abstraction.publish(mqtt_everest_prefix + "/ready", nlohmann::json(true));
            THEN("It should work") {
                std::string impl_id = "main";
                std::string var_name = "test_var";
                std::string test_value = "test_value";

                std::promise<nlohmann::json> promise;
                auto handler = [&promise](nlohmann::json data) {
                    promise.set_value(data);
                };
                auto token = std::make_shared<TypedHandler>(HandlerType::SubscribeVar, std::make_shared<Handler>(handler));
                const std::string mqtt_var_topic = mqtt_everest_prefix + "/" + module_id + "/" + impl_id + "/var";
                mqtt_abstraction.register_handler(mqtt_var_topic, token, QOS::QOS2);
                CHECK_NOTHROW(everest.publish_var(
                    impl_id,
                    var_name,
                    test_value
                ));
                std::future<nlohmann::json> future = promise.get_future();
                if (future.wait_for(std::chrono::seconds(1)) == std::future_status::timeout) {
                    FAIL("Timeout while waiting for var");
                } else {
                    CHECK(future.get() == test_value);
                }
            }
        }
    }
}
