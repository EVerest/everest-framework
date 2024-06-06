// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <utils/error/error_manager_impl.hpp>

#include <utils/error.hpp>
#include <utils/error/error_database.hpp>
#include <utils/error/error_exceptions.hpp>
#include <utils/error/error_type_map.hpp>

#include <everest/logging.hpp>

#include <list>
#include <memory>

namespace Everest {
namespace error {

ErrorManagerImpl::ErrorManagerImpl(std::shared_ptr<ErrorTypeMap> error_type_map_,
                                   std::shared_ptr<ErrorDatabase> error_database_,
                                   std::list<ErrorType> allowed_error_types_,
                                   ErrorManagerImpl::PublishErrorFunc publish_raised_error_,
                                   ErrorManagerImpl::PublishErrorFunc publish_cleared_error_,
                                   const bool validate_error_types_) :
    error_type_map(error_type_map_),
    database(error_database_),
    allowed_error_types(allowed_error_types_),
    publish_raised_error(publish_raised_error_),
    publish_cleared_error(publish_cleared_error_),
    validate_error_types(validate_error_types_) {
}

void ErrorManagerImpl::raise_error(const Error& error) {
    if (validate_error_types) {
        if (std::find(allowed_error_types.begin(), allowed_error_types.end(), error.type) ==
            allowed_error_types.end()) {
            throw EverestArgumentError("Error type " + error.type + " is not allowed to be raised.");
        }
        if (!this->error_type_map->has(error.type)) {
            throw EverestArgumentError("Error type " + error.type + " is not known.");
        }
    }
    if (!can_be_raised(error.type, error.sub_type)) {
        throw EverestArgumentError("Error type " + error.type + " is already active.");
    }
    database->add_error(std::make_shared<Error>(error));
    this->publish_raised_error(error);
    EVLOG_error << "Error raised, type: " << error.type << ", sub_type: " << error.sub_type
                << ", message: " << error.message;
}

std::list<ErrorPtr> ErrorManagerImpl::clear_error(const ErrorType& type, const bool clear_all) {
    if (!clear_all) {
        const ErrorSubType sub_type("");
        return clear_error(type, sub_type);
    }
    if (!can_be_cleared(type)) {
        throw EverestArgumentError("Errors can't be cleared, becauce type " + type + " is not active.");
    }
    std::list<ErrorFilter> filters = {ErrorFilter(TypeFilter(type))};
    std::list<ErrorPtr> res = database->remove_errors(filters);
    for (const ErrorPtr error : res) {
        this->publish_cleared_error(*error);
    }
    return res;
}

std::list<ErrorPtr> ErrorManagerImpl::clear_error(const ErrorType& type, const ErrorSubType& sub_type) {
    if (!can_be_cleared(type, sub_type)) {
        throw EverestArgumentError("Error can't be cleared, because it is not active.");
    }
    std::list<ErrorFilter> filters = {ErrorFilter(TypeFilter(type)), ErrorFilter(SubTypeFilter(sub_type))};
    std::list<ErrorPtr> res = database->remove_errors(filters);
    if (res.size() > 1) {
        throw EverestBaseLogicError("There are more than one matching error, this is not valid");
    }
    const ErrorPtr error = res.front();
    error->state = State::ClearedByModule;
    this->publish_cleared_error(*error);
    return res;
}

std::list<ErrorPtr> ErrorManagerImpl::clear_all_errors() {
    std::list<ErrorFilter> filters = {};
    std::list<ErrorPtr> res = database->remove_errors(filters);
    for (const ErrorPtr error : res) {
        error->state = State::ClearedByModule;
        this->publish_cleared_error(*error);
    }
    return res;
}

bool ErrorManagerImpl::can_be_raised(const ErrorType& type, const ErrorSubType& sub_type) const {
    std::list<ErrorFilter> filters = {ErrorFilter(TypeFilter(type)), ErrorFilter(SubTypeFilter(sub_type))};
    return database->get_errors(filters).empty();
}

bool ErrorManagerImpl::can_be_cleared(const ErrorType& type, const ErrorSubType& sub_type) const {
    std::list<ErrorFilter> filters = {ErrorFilter(TypeFilter(type)), ErrorFilter(SubTypeFilter(sub_type))};
    return !database->get_errors(filters).empty();
}

bool ErrorManagerImpl::can_be_cleared(const ErrorType& type) const {
    std::list<ErrorFilter> filters = {ErrorFilter(TypeFilter(type))};
    return !database->get_errors(filters).empty();
}

} // namespace error
} // namespace Everest
