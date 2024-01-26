// SPDX-License-Identifier: Apache-2.0
// Copyright 2024 Pionix GmbH and Contributors to EVerest
#include <everest/exceptions.hpp>
#include <utils/watchdog.hpp>

namespace Everest {

WatchdogSupervisor::WatchdogSupervisor(ModuleAdapter& ev) : ev(ev) {

    next_manager_feed_due = std::chrono::steady_clock::now();

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
            const auto now = std::chrono::steady_clock::now();
            if (next_manager_feed_due < now) {
                next_manager_feed_due = now + feed_manager_via_mqtt_interval;
                // send MQTT feed to manager
                this->ev.watchdog_feed_publish();
            }
        }
    });
}

WatchdogSupervisor::~WatchdogSupervisor() {
    should_stop = true;
    if (timeout_detection_thread.joinable()) {
        timeout_detection_thread.join();
    }
}

} // namespace Everest
