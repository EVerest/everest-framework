// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <utils/conversions.hpp>

namespace Everest {
namespace conversions {
constexpr auto CMD_EVENT_MESSAGE_PARSING = "MessageParsing";
constexpr auto CMD_EVENT_SCHEMA_VALIDATION = "SchemaValidation";
constexpr auto CMD_EVENT_HANDLER_EXCEPTION = "HandlerException";
constexpr auto CMD_EVENT_TIMEOUT = "Timeout";
constexpr auto CMD_EVENT_SHUTDOWN = "Shutdown";

std::string cmd_event_to_string(CmdEvent cmd_event) {
    switch (cmd_event) {
    case CmdEvent::MessageParsing:
        return CMD_EVENT_MESSAGE_PARSING;
        break;
    case CmdEvent::SchemaValidation:
        return CMD_EVENT_SCHEMA_VALIDATION;
        break;
    case CmdEvent::HandlerException:
        return CMD_EVENT_HANDLER_EXCEPTION;
        break;
    case CmdEvent::Timeout:
        return CMD_EVENT_TIMEOUT;
        break;
    case CmdEvent::Shutdown:
        return CMD_EVENT_SHUTDOWN;
        break;
    }

    throw std::runtime_error("Unknown CmdEvent");
}

CmdEvent string_to_cmd_event(const std::string& cmd_event_string) {
    if (cmd_event_string == CMD_EVENT_MESSAGE_PARSING) {
        return CmdEvent::MessageParsing;
    } else if (cmd_event_string == CMD_EVENT_SCHEMA_VALIDATION) {
        return CmdEvent::SchemaValidation;
    } else if (cmd_event_string == CMD_EVENT_HANDLER_EXCEPTION) {
        return CmdEvent::HandlerException;
    } else if (cmd_event_string == CMD_EVENT_TIMEOUT) {
        return CmdEvent::Timeout;
    } else if (cmd_event_string == CMD_EVENT_SHUTDOWN) {
        return CmdEvent::Shutdown;
    }

    throw std::runtime_error("Unknown CmdEvent");
}
} // namespace conversions

void to_json(nlohmann::json& j, const CmdResultError& e) {
    j = {{"type", conversions::cmd_event_to_string(e.event)}, {"msg", e.msg}};
}

void from_json(const nlohmann::json& j, CmdResultError& e) {
    e.event = conversions::string_to_cmd_event(j.at("type"));
    e.msg = j.at("msg");
}

} // namespace Everest
