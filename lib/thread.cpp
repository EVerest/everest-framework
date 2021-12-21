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
#include <utils/thread.hpp>

namespace Everest {
Thread::Thread() {
    exitFuture = exitSignal.get_future();
}

Thread::~Thread() {
    exitSignal.set_value();
    if (handle.joinable())
        handle.join();
}

bool Thread::shouldExit() {
    if (exitFuture.wait_for(std::chrono::seconds(0)) == std::future_status::ready)
        return true;
    return false;
}

void Thread::operator=(std::thread&& t) {
    handle = std::move(t);
}

} // namespace Everest
