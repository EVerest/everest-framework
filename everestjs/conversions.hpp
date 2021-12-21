/*
 * SPDX-License-Identifier: Apache-2.0
 *
 * Copyright 2020 - 2021 Pionix GmbH and Contributors to EVerest
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef CONVERSIONS_HPP
#define CONVERSIONS_HPP

#include <framework/everest.hpp>

#include <napi.h>

#include <utils/conversions.hpp>
#include <utils/types.hpp>

namespace EverestJs {

static const char* const napi_valuetype_strings[] = {
    "undefined", //
    "null",      //
    "boolean",   //
    "number",    //
    "string",    //
    "symbol",    //
    "object",    //
    "function",  //
    "external",  //
    "bigint",    //
};

Everest::json convertToJson(const Napi::Value& value);
Napi::Value convertToNapiValue(const Napi::Env& env, const Everest::json& value);

} // namespace EverestJs

#endif // CONVERSIONS_HPP
