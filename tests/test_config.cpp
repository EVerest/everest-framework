// SPDX-License-Identifier: Apache-2.0
// Copyright Pionix GmbH and Contributors to EVerest
#include <catch2/catch_all.hpp>

#include <utils/config.hpp>
#include <framework/runtime.hpp>

namespace fs = std::filesystem;

SCENARIO("Check RuntimeSetting Constructor", "[!throws]") {
    GIVEN("An invalid prefix, but a valid config file") {
        THEN("It should throw BootException") {
            CHECK_THROWS_AS(Everest::RuntimeSettings("non-valid-prefix/","valid_config/config.yaml"),
                Everest::BootException);
        }
    }
    GIVEN("A valid prefix, but a non existing config file") {
        THEN("It should throw BootException") {
            CHECK_THROWS_AS(Everest::RuntimeSettings("valid_config/","non-existing-config.yaml"),
                Everest::BootException);
        }
    }
    GIVEN("A valid prefix and a valid config file") {
        THEN("It should not throw") {
            CHECK_NOTHROW(Everest::RuntimeSettings("valid_config/","valid_config/config.yaml"));
        }
    }
    GIVEN("A broken config file") {
        THEN("It should throw Everest::EverestConfigError") {
            CHECK_THROWS_AS(Everest::RuntimeSettings("broken_config/","broken_config/config.yaml"),
                Everest::EverestConfigError);
        }
    }
}
SCENARIO("Check Config Constructor", "[!throws]") {
    GIVEN("An config without modules") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("empty_config/","empty_config/config.yaml"));
        Everest::Config config = Everest::Config(rs);
        THEN("It should not contain the module some_module") {
            CHECK(!config.contains("some_module"));
        }
        // THEN("It should not contain any module in active_modules") {
        //     CHECK(config.get_main_config().at("active_modules").size() == 0);
        // }
    }
    GIVEN("A config file referencing a non existent module") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("missing_module/","missing_module/config.yaml"));
        THEN("It should throw Everest::EverestConfigError") {
            CHECK_THROWS_AS(Everest::Config(rs),
                            Everest::EverestConfigError);
        }
    }
    GIVEN("A config file using a module with broken manifest (missing meta data)") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("broken_manifest_1/","broken_manifest_1/config.yaml"));
        THEN("It should throw Everest::EverestConfigError") {
            CHECK_THROWS_AS(Everest::Config(rs),
                            Everest::EverestConfigError);
        }
    }
    GIVEN("A config file using a module with broken manifest (empty file)") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("broken_manifest_2/","broken_manifest_2/config.yaml"));
        THEN("It should throw Everest::EverestConfigError") {
            // FIXME: an empty manifest breaks the test?
            CHECK_THROWS_AS(Everest::Config(rs),
                            Everest::EverestConfigError);
        }
    }
    GIVEN("A config file using a module with an invalid interface (missing interface)") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("missing_interface/","missing_interface/config.yaml"));
        THEN("It should throw Everest::EverestConfigError") {
            CHECK_THROWS_AS(Everest::Config(rs),
                            Everest::EverestConfigError);
        }
    }
    GIVEN("A valid config") {
        std::shared_ptr<Everest::RuntimeSettings> rs =
            std::make_shared<Everest::RuntimeSettings>(Everest::RuntimeSettings("valid_config/","valid_config/config.yaml"));
        THEN("It should not throw at all") {
            CHECK_NOTHROW(Everest::Config(rs));
        }
    }
}
