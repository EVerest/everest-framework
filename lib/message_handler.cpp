// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <utils/message_handler.hpp>

#include <everest/logging.hpp>
#include <fmt/format.h>

namespace Everest {

// Helper to split string by delimiter
std::vector<std::string> split_topic(const std::string& topic, char delimiter = '/') {
    std::vector<std::string> result;
    std::istringstream stream(topic);
    std::string part;
    while (std::getline(stream, part, delimiter)) {
        result.push_back(part);
    }
    return result;
}

// NOLINTNEXTLINE(misc-no-recursion)
bool check_topic_matches(const std::string& full_topic, const std::string& wildcard_topic) {
    // Verbatim match
    if (full_topic == wildcard_topic) {
        return true;
    }

    // Check if wildcard ends with "/#" and matches base
    if (wildcard_topic.size() >= 2 && wildcard_topic.compare(wildcard_topic.size() - 2, 2, "/#") == 0) {
        std::string start = wildcard_topic.substr(0, wildcard_topic.size() - 2);
        if (check_topic_matches(full_topic, start)) {
            return true;
        }
    }

    std::vector<std::string> full_split = split_topic(full_topic);
    std::vector<std::string> wildcard_split = split_topic(wildcard_topic);

    for (std::size_t partno = 0; partno < full_split.size(); ++partno) {
        if (partno >= wildcard_split.size()) {
            return false;
        }

        const std::string& full_part = full_split[partno];
        const std::string& wildcard_part = wildcard_split[partno];

        if (wildcard_part == "#") {
            return true;
        }

        if (wildcard_part == "+" || wildcard_part == full_part) {
            continue;
        }

        return false;
    }

    return full_split.size() == wildcard_split.size();
}

MessageHandler::MessageHandler() {
    main_worker_thread = std::thread([this] { run_main_worker(); });
    cmd_result_worker_thread = std::thread([this] { run_cmd_result_worker(); });
}

MessageHandler::~MessageHandler() {
    stop();
}

void MessageHandler::add(const ParsedMessage& message) {
    EVLOG_debug << "Adding message to queue: " << message.topic << " with data: " << message.data;

    std::string msg_type;

    // Defensive check: ensure message.data is an object before accessing keys
    if (message.data.is_object() && message.data.contains("msg_type")) {
        msg_type = message.data.at("msg_type").get<std::string>();
    } else {
        msg_type = "external_mqtt";
    }

    if (msg_type == "cmd_result") {
        EVLOG_verbose << "Pushing cmd_result message to queue: " << message.data;
        {
            std::lock_guard<std::mutex> lock(cmd_result_queue_mutex);
            cmd_result_queue.push(message);
        }
        cmd_result_cv.notify_all();
    } else if (msg_type == "global_ready") {
        const auto topic_copy = message.topic;
        const auto data_copy = message.data.at("data");

        ready_thread =
            std::thread([this, topic_copy, data_copy] { (*global_ready_handler->handler)(topic_copy, data_copy); });
    } else {
        {
            std::lock_guard<std::mutex> lock(main_queue_mutex);
            main_queue.push(message);
        }
        main_cv.notify_all();
    }
}

void MessageHandler::stop() {
    {
        std::lock_guard<std::mutex> lock1(main_queue_mutex);
        std::lock_guard<std::mutex> lock2(cmd_result_queue_mutex);
        running = false;
    }

    main_cv.notify_all();
    cmd_result_cv.notify_all();

    if (main_worker_thread.joinable()) {
        main_worker_thread.join();
    }
    if (cmd_result_worker_thread.joinable()) {
        cmd_result_worker_thread.join();
    }
    if (ready_thread.joinable()) {
        ready_thread.join();
    }
}

void MessageHandler::run_main_worker() {
    while (true) {
        std::unique_lock<std::mutex> lock(main_queue_mutex);
        main_cv.wait(lock, [this] { return !main_queue.empty() || !running; });
        if (!running)
            return;

        ParsedMessage message = std::move(main_queue.front());
        main_queue.pop();
        lock.unlock();

        handle_message(message.topic, message.data);
    }
    EVLOG_info << "Main worker thread stopped";
}

void MessageHandler::run_cmd_result_worker() {
    while (true) {
        std::unique_lock<std::mutex> lock(cmd_result_queue_mutex);
        cmd_result_cv.wait(lock, [this] { return !cmd_result_queue.empty() || !running; });
        if (!running)
            return;

        ParsedMessage message = std::move(cmd_result_queue.front());
        cmd_result_queue.pop();
        lock.unlock();

        handle_cmd_result_message(message.topic, message.data);
    }
    EVLOG_info << "Cmd result worker thread stopped";
}

void MessageHandler::handle_message(const std::string& topic, const json& payload) {
    json data;
    MqttMessageType msg_type;

    // Determine message type
    if (!payload.contains("msg_type")) {
        msg_type = MqttMessageType::ExternalMQTT;
    } else {
        msg_type = string_to_mqtt_message_type(payload.at("msg_type").get<std::string>());
    }

    // Determine payload structure
    if (payload.contains("data")) {
        data = payload.at("data");
    } else {
        data = payload;
    }

    switch (msg_type) {
    case MqttMessageType::Var: {
        std::vector<std::shared_ptr<TypedHandler>> handlers_copy;
        {
            std::lock_guard<std::mutex> lock(handler_mutex);
            const auto it = var_handlers.find(topic);
            if (it != var_handlers.end()) {
                handlers_copy = it->second;
            }
        }
        for (const auto& handler : handlers_copy) {
            (*handler->handler)(topic, data.at("data"));
        }
        break;
    }
    case MqttMessageType::Cmd: {
        std::lock_guard<std::mutex> lock(handler_mutex);
        (*cmd_handlers.at(topic)->handler)(topic, data);
        break;
    }
    case MqttMessageType::ExternalMQTT: {
        std::vector<std::shared_ptr<TypedHandler>> handlers_copy;
        {
            std::lock_guard<std::mutex> lock(handler_mutex);
            const auto it = external_var_handlers.find(topic);
            if (it != external_var_handlers.end()) {
                handlers_copy = it->second;
            }
        }
        for (const auto& handler : handlers_copy) {
            (*handler->handler)(topic, data);
        }
        break;
    }
    case MqttMessageType::RaiseError:
    case MqttMessageType::ClearError: {
        std::lock_guard<std::mutex> lock(handler_mutex);
        for (const auto& [_topic, error_handler] : error_handlers) {
            if (check_topic_matches(topic, _topic)) {
                (*error_handler->handler)(topic, data);
            }
        }
        break;
    }
    case MqttMessageType::GetConfig: {
        std::lock_guard<std::mutex> lock(handler_mutex);
        const auto it = get_module_config_handlers.find(topic);
        if (it != get_module_config_handlers.end()) {
            (*it->second->handler)(topic, data);
        }
        break;
    }
    case MqttMessageType::GetConfigResponse:
    case MqttMessageType::Metadata:
    case MqttMessageType::Telemetry:
    case MqttMessageType::Interfaces:
    case MqttMessageType::InterfaceDefinitions:
    case MqttMessageType::Types:
    case MqttMessageType::Schemas:
    case MqttMessageType::Settings:
    case MqttMessageType::Manifests:
    case MqttMessageType::ModuleNames:
    case MqttMessageType::TypeDefinitions: {
        std::lock_guard<std::mutex> lock(handler_mutex);
        (*config_response_handler->handler)(topic, data);
        break;
    }
    case MqttMessageType::ModuleReady: {
        std::lock_guard<std::mutex> lock(handler_mutex);
        (*module_ready_handlers.at(topic)->handler)(topic, data);
        break;
    }
    default:
        break;
    }
}

void MessageHandler::handle_cmd_result_message(const std::string& topic, const json& payload) {
    if (payload.contains("msg_type") &&
        string_to_mqtt_message_type(payload.at("msg_type")) == MqttMessageType::CmdResult) {
        const auto& data = payload.at("data").at("data");
        const auto id = data.at("id").get<std::string>();
        {
            std::lock_guard<std::mutex> lock(cmd_result_handler_mutex);
            auto it = cmd_result_handlers.find(id);
            if (it != cmd_result_handlers.end()) {
                (*it->second->handler)(topic, data);
                cmd_result_handlers.erase(it);
            }
        }
    } else {
        EVLOG_warning << "Received invalid cmd_result message: " << payload;
    }
}

void MessageHandler::register_handler(const std::string& topic, std::shared_ptr<TypedHandler> handler) {
    switch (handler->type) {
    case HandlerType::Call: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        cmd_handlers[topic] = handler;
        break;
    }
    case HandlerType::Result: {
        std::lock_guard<std::mutex> lock(cmd_result_handler_mutex);
        cmd_result_handlers[handler->id] = handler;
        break;
    }
    case HandlerType::SubscribeVar: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        var_handlers[topic].push_back(handler);
        break;
    }
    case HandlerType::SubscribeError: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        error_handlers[topic] = handler;
        break;
    }
    case HandlerType::ExternalMQTT: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        external_var_handlers[topic].push_back(handler);
        break;
    }
    case HandlerType::GetConfig: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        get_module_config_handlers[topic] = handler;
        break;
    }
    case HandlerType::GetConfigResponse: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        config_response_handler = handler;
        break;
    }
    case HandlerType::ModuleReady: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        module_ready_handlers[topic] = handler;
        break;
    }
    case HandlerType::GlobalReady: {
        std::lock_guard<std::mutex> lg(handler_mutex);
        global_ready_handler = handler;
        break;
    }
    default:
        EVLOG_warning << "Unknown handler type for topic: " << topic;
        break;
    }
}

} // namespace Everest
