// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2025 Pionix GmbH and Contributors to EVerest

#include <nlohmann/json.hpp>
#include <utils/config/types.hpp>

using json = nlohmann::json;

namespace everest::config {

ConfigEntry ConfigurationParameter::get_typed_value() const {
    switch (characteristics.datatype) {
    case Datatype::String:
        return value;
    case Datatype::Decimal:
        return std::stod(value);
    case Datatype::Integer:
        return std::stoi(value);
    case Datatype::Boolean:
        return (value == "true" || value == "1");
    case Datatype::Path:
        return fs::path(value);
    default:
        throw std::runtime_error("Unsupported datatype");
    }
}

Datatype string_to_datatype(const std::string& str) {
    if (str == "string") {
        return Datatype::String;
    } else if (str == "number") {
        return Datatype::Decimal;
    } else if (str == "integer") {
        return Datatype::Integer;
    } else if (str == "boolean") {
        return Datatype::Boolean;
    } else if (str == "path") {
        return Datatype::Path;
    }
    throw std::out_of_range("Could not convert: " + str + " to Datatype");
}

std::string datatype_to_string(const Datatype datatype) {
    switch (datatype) {
    case Datatype::String:
        return "string";
    case Datatype::Decimal:
        return "number";
    case Datatype::Integer:
        return "integer";
    case Datatype::Boolean:
        return "bool";
    case Datatype::Path:
        return "path";
    }
    throw std::out_of_range("Could not convert Datatype to string");
}

Mutability string_to_mutability(const std::string& str) {
    if (str == "ReadOnly") {
        return Mutability::ReadOnly;
    } else if (str == "ReadWrite") {
        return Mutability::ReadWrite;
    } else if (str == "WriteOnly") {
        return Mutability::WriteOnly;
    }
    throw std::out_of_range("Could not convert: " + str + " to Mutability");
}

std::string mutability_to_string(const Mutability mutability) {
    switch (mutability) {
    case Mutability::ReadOnly:
        return "ReadOnly";
    case Mutability::ReadWrite:
        return "ReadWrite";
    case Mutability::WriteOnly:
        return "WriteOnly";
    }
    throw std::out_of_range("Could not convert Mutability to string");
}

} // namespace everest::config
