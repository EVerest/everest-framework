// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#ifndef TESTS_HELPERS_HPP

#include <filesystem>

namespace fs = std::filesystem;

namespace everest{
namespace tests{

fs::path get_bin_dir();
std::string get_unique_mqtt_test_prefix();

} // namespace tests
} // namespace everest

#endif // TESTS_HELPERS_HPP
