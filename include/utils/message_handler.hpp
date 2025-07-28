// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#pragma once

#include <condition_variable>
#include <map>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>

#include <utils/message_queue.hpp>
#include <utils/types.hpp>

using MqttTopic = std::string;
using CmdId = std::string;

namespace Everest {

/// \brief Handles message dispatching and thread-safe queuing of different message types
class MessageHandler {
public:
    MessageHandler();
    ~MessageHandler();

    /// \brief Adds given \p message to the message queue for processing
    void add(const ParsedMessage& message);

    /// \brief Stops all threads started by this handler
    void stop();

    /// \brief Registers a \p handler for a specific \p topic
    void register_handler(const std::string& topic, std::shared_ptr<TypedHandler> handler);

private:
    void run_main_worker();
    void run_cmd_result_worker();

    void handle_message(const std::string& topic, const json& payload);
    void handle_cmd_result_message(const std::string& topic, const json& payload);

    // Threads
    std::thread main_worker_thread;
    std::thread cmd_result_worker_thread;
    std::thread ready_thread;

    // Queues and sync primitives
    std::queue<ParsedMessage> main_queue;
    std::queue<ParsedMessage> cmd_result_queue;

    std::mutex main_queue_mutex;
    std::condition_variable main_cv;

    std::mutex cmd_result_queue_mutex;
    std::condition_variable cmd_result_cv;

    std::mutex cmd_result_handler_mutex;
    std::mutex handler_mutex;

    bool running = true;

    // Handler data structures
    std::map<MqttTopic, std::vector<std::shared_ptr<TypedHandler>>> var_handlers; // var handlers of module
    std::map<MqttTopic, std::shared_ptr<TypedHandler>> cmd_handlers;              // cmd handlers of module
    std::map<CmdId, std::shared_ptr<TypedHandler>> cmd_result_handlers;           // cmd result handlers of module
    std::map<MqttTopic, std::shared_ptr<TypedHandler>> error_handlers;            // error handlers of module
    std::map<MqttTopic, std::shared_ptr<TypedHandler>>
        get_module_config_handlers;                        // get module config handler of manager
    std::shared_ptr<TypedHandler> config_response_handler; // get module config response handler of module
    std::shared_ptr<TypedHandler> global_ready_handler;    // global ready handler of module
    std::map<MqttTopic, std::shared_ptr<TypedHandler>> module_ready_handlers; // module ready handlers of manager
    std::map<MqttTopic, std::vector<std::shared_ptr<TypedHandler>>>
        external_var_handlers; // external MQTT handlers of module
};

} // namespace Everest
