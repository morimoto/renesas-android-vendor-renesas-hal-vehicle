/*
 * Copyright (C) 2017 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "VehicleHalImpl"

#include <iostream>

#include <android-base/logging.h>
#include <android-base/macros.h>

#include <hidl/HidlTransportSupport.h>
#include <vhal_v2_0/VehicleHalManager.h>

#include "UserHalImpl.h"
#include "VehicleHalImpl.h"

using namespace android;
using namespace android::hardware;
using namespace android::hardware::automotive::vehicle::V2_0;

int main(int /* argc */, char* /* argv */ []) {
    auto store = std::make_unique<VehiclePropertyStore>();
    auto userHal = std::make_unique<renesas::UserHal>();
    auto hal = std::make_unique<renesas::VehicleHalImpl>(store.get(),
                                                         userHal.get());
    auto service = std::make_unique<VehicleHalManager>(hal.get());

    configureRpcThreadpool(4, true /* callerWillJoin */);

    CHECK_EQ(service->registerAsService(), android::NO_ERROR)
      << "Failed to register vehicle HAL";

    joinRpcThreadpool();
}
