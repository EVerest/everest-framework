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

namespace Everest {

class WatchdogSupervisor {
public:
    WatchdogSupervisor() {
        // Thread that monitors all registered watchdogs
        timeout_detection_thread = std::thread([this]() {
            while (not should_stop) {
                std::this_thread::sleep_for(check_interval);

                // check if any watchdog timed out
                for (const auto& dog : dogs) {
                    std::chrono::steady_clock::time_point last_seen = dog.last_seen;
                    if (std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - last_seen) >
                        dog.timeout) {
                        // Throw exception
                        EVLOG_AND_THROW(EverestTimeoutError("Module internal watchdog timeout: " + dog.description));
                    }
                }

                // check if we need to send an MQTT feed to manager
                if (--feed_manager_via_mqtt_countdown == 0) {
                    feed_manager_via_mqtt_countdown = feed_manager_via_mqtt_counts_max;
                    // send MQTT feed to manager
                }
            }
        });
    }

    ~WatchdogSupervisor() {
        should_stop = true;
        if (timeout_detection_thread.joinable()) {
            timeout_detection_thread.join();
        }
    }

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
    constexpr static int feed_manager_via_mqtt_counts_max = feed_manager_via_mqtt_interval / check_interval;
};

} // namespace Everest

#endif
