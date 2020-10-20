/*
 * Copyright (C) 2020 GlobalLogic
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

#ifndef _VEHICLE_USERHALIMPL_H
#define _VEHICLE_USERHALIMPL_H

#include <android-base/result.h>
#include <android/hardware/automotive/vehicle/2.0/types.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {

namespace renesas {

class UserHal {
  public:
    UserHal() {}

    ~UserHal() = default;

    /**
     * Checks if the user HAL can handle the property.
     */
    bool isSupported(int32_t prop) const;

    /**
     * Lets the user HAL set the property.
     *
     * @return updated property and StatusCode
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onSetProperty(
            const VehiclePropValue& value);

    /**
     * Gets the property value from the user HAL.
     *
     * @return property value and StatusCode
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onGetProperty(
            const VehiclePropValue& value);

  private:
    /**
     * INITIAL_USER_INFO is called by Android when it starts, and it's expecting a property change
     * indicating what the initial user should be.
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onSetInitialUserInfoResponse(
            const VehiclePropValue& value);

    /**
     * Used to handle SWITCH_USER query.
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onSetSwitchUserResponse(
            const VehiclePropValue& value);

    /**
     * Used to handle CREATE_USER query.
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onSetCreateUserResponse(
            const VehiclePropValue& value);

    /**
     * Used to handle USER_IDENTIFICATION_ASSOCIATION set query.
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onSetUserIdentificationAssociation(
            const VehiclePropValue& value);

    /**
     * Used to handle USER_IDENTIFICATION_ASSOCIATION get query.
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> onGetUserIdentificationAssociation(
            const VehiclePropValue& value);

    /**
     * Creates a default USER_IDENTIFICATION_ASSOCIATION response.
     */
    android::base::Result<std::unique_ptr<VehiclePropValue>> defaultUserIdentificationAssociation(
            const VehiclePropValue& request);
};

}  // namespace renesas

}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif  // _VEHICLE_USERHALIMPL_H
