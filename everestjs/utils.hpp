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
#ifndef UTILS_HPP
#define UTILS_HPP

#include <everest/logging.hpp>
#include <everest/metamacros.hpp>

namespace EverestJs {

// this is needed to get javascript stacktraces whenever possible while still logging boost error information
#define EVLOG_AND_RETHROW(...)                                                                                         \
    do {                                                                                                               \
        try {                                                                                                          \
            throw;                                                                                                     \
        } catch (Napi::Error & e) {                                                                                    \
            try {                                                                                                      \
                BOOST_THROW_EXCEPTION(boost::enable_error_info(e)                                                      \
                                      << boost::log::BOOST_LOG_VERSION_NAMESPACE::current_scope());                    \
            } catch (std::exception & e) {                                                                             \
                EVLOG(critical) << "Catched top level Napi::Error, forwarding to javascript..." << std::endl           \
                                << boost::diagnostic_information(e, true) << std::endl                                 \
                                << "==============================" << std::endl                                       \
                                << std::endl;                                                                          \
            }                                                                                                          \
            throw;                                                                                                     \
        } catch (std::runtime_error & e) {                                                                             \
            try {                                                                                                      \
                BOOST_THROW_EXCEPTION(boost::enable_error_info(e)                                                      \
                                      << boost::log::BOOST_LOG_VERSION_NAMESPACE::current_scope());                    \
            } catch (std::exception & e) {                                                                             \
                EVLOG(critical) << "Catched top level runtime exception, forwarding to javascript..." << std::endl     \
                                << boost::diagnostic_information(e, true) << std::endl                                 \
                                << "==============================" << std::endl                                       \
                                << std::endl;                                                                          \
                metamacro_if_eq(0, metamacro_argcount(__VA_ARGS__))(throw;)(EVTHROW(Napi::Error::New(                  \
                    metamacro_at(0, __VA_ARGS__), Napi::String::New(metamacro_at(0, __VA_ARGS__), e.what())));)        \
            }                                                                                                          \
        } catch (std::logic_error & e) {                                                                               \
            try {                                                                                                      \
                BOOST_THROW_EXCEPTION(boost::enable_error_info(e)                                                      \
                                      << boost::log::BOOST_LOG_VERSION_NAMESPACE::current_scope());                    \
            } catch (std::exception & e) {                                                                             \
                EVLOG(critical) << "Catched top level logic exception, forwarding to javascript..." << std::endl       \
                                << boost::diagnostic_information(e, true) << std::endl                                 \
                                << "==============================" << std::endl                                       \
                                << std::endl;                                                                          \
                metamacro_if_eq(0, metamacro_argcount(__VA_ARGS__))(throw;)(EVTHROW(Napi::Error::New(                  \
                    metamacro_at(0, __VA_ARGS__), Napi::String::New(metamacro_at(0, __VA_ARGS__), e.what())));)        \
            }                                                                                                          \
        } catch (std::exception & e) {                                                                                 \
            try {                                                                                                      \
                BOOST_THROW_EXCEPTION(boost::enable_error_info(e)                                                      \
                                      << boost::log::BOOST_LOG_VERSION_NAMESPACE::current_scope());                    \
            } catch (std::exception & e) {                                                                             \
                EVLOG(critical) << "Catched top level exception, forwarding to javascript..." << std::endl             \
                                << boost::diagnostic_information(e, true) << std::endl                                 \
                                << "==============================" << std::endl                                       \
                                << std::endl;                                                                          \
                throw;                                                                                                 \
            }                                                                                                          \
        }                                                                                                              \
    } while (0)
} // namespace EverestJs

#endif // UTILS_HPP
