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
#ifndef UTILS_CONVERSIONS_HPP
#define UTILS_CONVERSIONS_HPP

#include <nlohmann/json.hpp>

#include <utils/types.hpp>

namespace Everest {
using json = nlohmann::json;

template <class R, class V> R convertTo(V);

template <> json convertTo<json>(Result retval);
template <> Result convertTo<Result>(json data);
template <> json convertTo<json>(Parameters params);
template <> Parameters convertTo<Parameters>(json data);
template <> json convertTo<json>(Value value);
template <> Value convertTo<Value>(json data);

} // namespace Everest

#endif // UTILS_CONVERSIONS_HPP
