// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Pionix GmbH and Contributors to EVerest

/*
 This is a helper class for modules to have an everest internal watchdog system.
 A module can register watchdogs for several threads and feed them.
 Requires external_mqtt enabled in the manifest.

 A special watchdog module can check all watchdogs and itself feed e.g. a systemd watchdog.
*/

#ifndef EVEREST_UTILS_WATCHDOG_HPP
#define EVEREST_UTILS_WATCHDOG_HPP

#include <atomic>
#include <chrono>
#include <list>
#include <mutex>
#include <ratio>
#include <string>
#include <thread>

#include <everest/logging.hpp>
#include <framework/ModuleAdapter.hpp>

namespace Everest {

class WatchdogSupervisor {
public:
    WatchdogSupervisor(ModuleAdapter& ev);

    ~WatchdogSupervisor();

    auto register_watchdog(const std::string& description, std::chrono::seconds timeout) {
        std::scoped_lock lock(dogs_lock);
        dogs.emplace_back(description, timeout);
        auto& dog = dogs.back();
        return [&dog]() { dog.last_seen = std::chrono::steady_clock::now(); };
    }

private:
    struct WatchdogData {
        WatchdogData(const std::string& _description, std::chrono::seconds _timeout) :
            description(_description), timeout(_timeout) {
            last_seen = std::chrono::steady_clock::now();
        }
        const std::string description;
        const std::chrono::seconds timeout;
        std::atomic<std::chrono::steady_clock::time_point> last_seen;
    };

    std::mutex dogs_lock;
    std::list<WatchdogData> dogs;
    std::atomic_bool should_stop{false};
    std::thread timeout_detection_thread;
    int feed_manager_via_mqtt_countdown{0};

    constexpr static std::chrono::seconds check_interval{1};
    constexpr static std::chrono::seconds feed_manager_via_mqtt_interval{15};
    std::chrono::steady_clock::time_point next_manager_feed_due;

    ModuleAdapter& ev;
};

} // namespace Everest

#endif
