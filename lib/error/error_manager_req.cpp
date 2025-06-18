// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <utility>
#include <utils/error/error_manager_req.hpp>

#include <utils/error.hpp>
#include <utils/error/error_database.hpp>
#include <utils/error/error_filter.hpp>
#include <utils/error/error_json.hpp>
#include <utils/error/error_type_map.hpp>

#include <everest/logging.hpp>

namespace Everest {
namespace error {

ErrorManagerReq::ErrorManagerReq(std::shared_ptr<ErrorTypeMap> error_type_map_,
                                 std::shared_ptr<ErrorDatabase> error_database_,
                                 std::list<ErrorType> allowed_error_types_, SubscribeErrorFunc subscribe_error_func_) :
    error_type_map(error_type_map_),
    database(error_database_),
    allowed_error_types(std::move(allowed_error_types_)),
    subscribe_error_func(std::move(subscribe_error_func_)) {

    for (const ErrorType& type : allowed_error_types) {
        if (!error_type_map->has(type)) {
            EVLOG_error << "Error type '" << type << "' in allowed_error_types is not defined, ignored.";
        }
    }
    const ErrorCallback on_raise = [this](const Error& error) { this->on_error_raised(error); };
    const ErrorCallback on_clear = [this](const Error& error) { this->on_error_cleared(error); };
    for (const ErrorType& type : allowed_error_types) {
        subscribe_error_func(type, on_raise, on_clear);
        error_subscriptions[type] = {};
    }
}

ErrorManagerReq::Subscription::Subscription(const ErrorType& type_, const ErrorCallback& callback_,
                                            const ErrorCallback& clear_callback_) :
    type(type_), callback(callback_), clear_callback(clear_callback_) {
}

void ErrorManagerReq::subscribe_error(const ErrorType& type, const ErrorCallback& callback,
                                      const ErrorCallback& clear_callback) {
    if (error_subscriptions.count(type) != 1) {
        EVLOG_error << "Tpye " << type << " is not known, ignore subscription";
        return;
    }
    const Subscription sub(type, callback, clear_callback);
    error_subscriptions.at(type).push_back(sub);
}

void ErrorManagerReq::subscribe_all_errors(const ErrorCallback& callback, const ErrorCallback& clear_callback) {
    for (const ErrorType& type : allowed_error_types) {
        const Subscription sub(type, callback, clear_callback);
        error_subscriptions.at(type).push_back(sub);
    }
}

void ErrorManagerReq::on_error_raised(const Error& error) {
    if (std::find(allowed_error_types.begin(), allowed_error_types.end(), error.type) == allowed_error_types.end()) {
        std::stringstream ss;
        ss << "Error can't be raised by module_id " << error.origin.module_id << " and implemenetation_id "
           << error.origin.implementation_id << ", ignored.";
        ss << std::endl << "Error object: " << nlohmann::json(error).dump(2);
        EVLOG_error << ss.str();
        return;
    }
    if (!error_type_map->has(error.type)) {
        std::stringstream ss;
        ss << "Error type '" << error.type << "' is not defined, ignored.";
        ss << std::endl << "Error object: " << nlohmann::json(error).dump(2);
        EVLOG_error << ss.str();
        return;
    }
    database->add_error(std::make_shared<Error>(error));
    on_error(error, true);
}

void ErrorManagerReq::on_error_cleared(const Error& error) {
    const std::list<ErrorFilter> filters = {ErrorFilter(TypeFilter(error.type)), ErrorFilter(SubTypeFilter(error.sub_type))};
    const std::list<ErrorPtr> res = database->remove_errors(filters);
    if (res.size() < 1) {
        std::stringstream ss;
        ss << "Error wasn't raised, type: " << error.type << ", sub_type: " << error.sub_type << ", ignored.";
        ss << std::endl << "Error object: " << nlohmann::json(error).dump(2);
        EVLOG_error << ss.str();
        return;
    }
    if (res.size() > 1) {
        std::stringstream ss;
        ss << "More than one error is cleared, type: " << error.type << ", sub_type: " << error.sub_type
           << ", ignored.";
        for (const ErrorPtr& error : res) {
            ss << std::endl << "Error object: " << nlohmann::json(*error).dump(2);
        }
        EVLOG_error << ss.str();
        return;
    }

    on_error(error, false);
}

void ErrorManagerReq::on_error(const Error& error, const bool raised) const {
    for (const Subscription& sub : error_subscriptions.at(error.type)) {
        if (raised) {
            sub.callback(error);
        } else {
            sub.clear_callback(error);
        }
    }
}

} // namespace error
} // namespace Everest
