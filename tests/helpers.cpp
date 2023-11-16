// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest

#include <tests/helpers.hpp>

#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/uuid/uuid_io.hpp>

namespace everest{
namespace tests{
fs::path get_bin_dir() {
    return fs::canonical("/proc/self/exe").parent_path();
}

std::string get_unique_mqtt_test_prefix() {
    const std::string prefix = "test-everest-";
    const std::string uuid = boost::uuids::to_string(boost::uuids::random_generator()());
    return prefix + uuid;
}

} // namespace tests
} // namespace everest
