// SPDX-License-Identifier: Apache-2.0
// Copyright 2020 - 2022 Pionix GmbH and Contributors to EVerest
#ifndef UTILS_EXIT_HANDLER_HPP
#define UTILS_EXIT_HANDLER_HPP

#include <date/date.h>
#include <date/tz.h>

namespace Everest {
namespace ExitHandler {

void exit_handler(const int sig_num);

} // namespace ExitHandler
} // namespace Everest

#endif // UTILS_EXIT_HANDLER_HPP
