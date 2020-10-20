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
#define LOG_TAG "VehicleHalImpl_UserHal"

#include "UserHalImpl.h"

#include <cutils/log.h>
#include <utils/SystemClock.h>
#include <vhal_v2_0/VehicleUtils.h>

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {

namespace renesas {

enum class requestType : int32_t {
    SET = 0,
    GET
};

constexpr int32_t INITIAL_USER_INFO =
    static_cast<int32_t>(VehicleProperty::INITIAL_USER_INFO);
constexpr int32_t SWITCH_USER =
    static_cast<int32_t>(VehicleProperty::SWITCH_USER);
constexpr int32_t CREATE_USER =
    static_cast<int32_t>(VehicleProperty::CREATE_USER);
constexpr int32_t REMOVE_USER =
    static_cast<int32_t>(VehicleProperty::REMOVE_USER);
constexpr int32_t USER_IDENTIFICATION_ASSOCIATION =
    static_cast<int32_t>(VehicleProperty::USER_IDENTIFICATION_ASSOCIATION);
constexpr int32_t NOT_ASSOCIATED_ANY_USER =
    static_cast<int32_t>(UserIdentificationAssociationValue
                                        ::NOT_ASSOCIATED_ANY_USER);
constexpr int INVALID_ARG =
    static_cast<int>(StatusCode::INVALID_ARG);

bool UserHal::isSupported(int32_t prop) const{
    switch (prop) {
        case INITIAL_USER_INFO:
        case SWITCH_USER:
        case CREATE_USER:
        case REMOVE_USER:
        case USER_IDENTIFICATION_ASSOCIATION:
            return true;
        default:
            return false;
    }
}

android::base::Result<std::unique_ptr<VehiclePropValue>> UserHal::onSetProperty(
    const VehiclePropValue& value) {
    ALOGV("onSetProperty(): %s", toString(value).c_str());

    switch (value.prop) {
        case INITIAL_USER_INFO:
            return onSetInitialUserInfoResponse(value);
        case SWITCH_USER:
            return onSetSwitchUserResponse(value);
        case CREATE_USER:
            return onSetCreateUserResponse(value);
        case REMOVE_USER:
            ALOGI("REMOVE_USER is FYI only, nothing to do...");
            return {};
        case USER_IDENTIFICATION_ASSOCIATION:
            return onSetUserIdentificationAssociation(value);
        default:
            return android::base::Error(INVALID_ARG)
                   << "Unsupported property: " << toString(value);
    }
}

android::base::Result<std::unique_ptr<VehiclePropValue>> UserHal::onGetProperty(
    const VehiclePropValue& value) {
    ALOGV("onGetProperty(%s)", toString(value).c_str());
    switch (value.prop) {
        case INITIAL_USER_INFO:
        case SWITCH_USER:
        case CREATE_USER:
        case REMOVE_USER:
            ALOGE("onGetProperty(): %d is only supported on SET", value.prop);
            return android::base::Error(INVALID_ARG)
                   << "only supported on SET";
        case USER_IDENTIFICATION_ASSOCIATION:
            return onGetUserIdentificationAssociation(value);
        default:
            ALOGE("onGetProperty(): %d is not supported", value.prop);
            return android::base::Error(INVALID_ARG)
                   << "not supported by User HAL";
    }
}

/**
*   Only for USER_IDENTIFICATION_ASSOCIATION requests
*   Used for create default UserIdentificationAssociation response
*/
std::unique_ptr<VehiclePropValue>
getListAssociationTypes(const VehiclePropValue& value, requestType type) {
    int32_t requestId = value.value.int32Values[0];
    int32_t numTypesQueried = value.value.int32Values[3];
    auto updatedValue = createVehiclePropValue(VehiclePropertyType::INT32,
                                               numTypesQueried + 2);

    updatedValue->prop = USER_IDENTIFICATION_ASSOCIATION;
    updatedValue->status = value.status;
    updatedValue->timestamp = value.timestamp;
    updatedValue->value.int32Values[0] = requestId;
    updatedValue->value.int32Values[1] = numTypesQueried;
    switch (type) {
        case requestType::GET:
            for (size_t i = 4; i < value.value.int32Values.size(); ++i) {
                updatedValue->value.int32Values[i - 2] = value.value.int32Values[i];
            }
            break;
        case requestType::SET:
            for (size_t i = 4, j = 2; i < value.value.int32Values.size(); i += 2, ++j) {
                updatedValue->value.int32Values[j] = value.value.int32Values[i];
            }
            break;

        default:
            ALOGW("Unsupported type of request");
            updatedValue.reset(nullptr);
            break;
    }

    return updatedValue;
}

android::base::Result<std::unique_ptr<VehiclePropValue>>
UserHal::onGetUserIdentificationAssociation(const VehiclePropValue& value) {
    if (value.value.int32Values.size() == 0) {
        ALOGE("get(USER_IDENTIFICATION_ASSOCIATION): no int32values, ignoring it: %s",
              toString(value).c_str());
        return android::base::Error(INVALID_ARG)
               << "no int32values on " << toString(value);
    }

    ALOGI("get(USER_IDENTIFICATION_ASSOCIATION) called from Android: %s",
          toString(value).c_str());
    auto updatedValue = getListAssociationTypes(value, requestType::GET);

    if (updatedValue == nullptr) {
        return android::base::Error(INVALID_ARG) << "Bad arguments";
    }

    // Returns default response
    return defaultUserIdentificationAssociation(*updatedValue.get());
}

android::base::Result<std::unique_ptr<VehiclePropValue>>
UserHal::onSetInitialUserInfoResponse(const VehiclePropValue& value) {
    if (value.value.int32Values.size() == 0) {
        ALOGE("set(INITIAL_USER_INFO): no int32values, ignoring it: %s",
              toString(value).c_str());
        return android::base::Error(INVALID_ARG)
               << "no int32values on " << toString(value);
    }

    ALOGI("set(INITIAL_USER_INFO) called from Android: %s", toString(value).c_str());
    int32_t requestId = value.value.int32Values[0];
    // Returns default response
    auto updatedValue = createVehiclePropValue(VehiclePropertyType::INT32, 2);

    updatedValue->prop = INITIAL_USER_INFO;
    updatedValue->timestamp = value.timestamp;
    updatedValue->status = value.status;
    updatedValue->value.int32Values[0] = requestId;
    updatedValue->value.int32Values[1] =
        static_cast<int32_t>(InitialUserInfoResponseAction::DEFAULT);

    return updatedValue;
}

android::base::Result<std::unique_ptr<VehiclePropValue>>
UserHal::onSetSwitchUserResponse(
    const VehiclePropValue& value) {
    if (value.value.int32Values.size() == 0) {
        ALOGE("set(SWITCH_USER): no int32values, ignoring it: %s",
              toString(value).c_str());
        return android::base::Error(INVALID_ARG)
               << "no int32values on " << toString(value);
    }

    ALOGI("set(SWITCH_USER) called from Android: %s", toString(value).c_str());
    if (value.value.int32Values.size() > 1) {
        auto messageType =
            static_cast<SwitchUserMessageType>(value.value.int32Values[1]);
        switch (messageType) {
            case SwitchUserMessageType::LEGACY_ANDROID_SWITCH:
                ALOGI("request is LEGACY_ANDROID_SWITCH; ignoring it");
                return {};
            case SwitchUserMessageType::ANDROID_POST_SWITCH:
                ALOGI("request is ANDROID_POST_SWITCH; ignoring it");
                return {};
            case SwitchUserMessageType::VEHICLE_REQUEST:
                ALOGI("request is VEHICLE_REQUEST; pass the request on");
                return std::unique_ptr<VehiclePropValue>(new VehiclePropValue(value));
            default:
                break;
        }
    }

    int32_t requestId = value.value.int32Values[0];
    // Returns default response
    auto updatedValue = createVehiclePropValue(VehiclePropertyType::INT32, 3);
    updatedValue->prop = SWITCH_USER;
    updatedValue->timestamp = value.timestamp;
    updatedValue->status = value.status;
    updatedValue->value.int32Values[0] = requestId;
    updatedValue->value.int32Values[1] =
        static_cast<int32_t>(SwitchUserMessageType::VEHICLE_RESPONSE);
    updatedValue->value.int32Values[2] =
        static_cast<int32_t>(SwitchUserStatus::SUCCESS);

    return updatedValue;
}

android::base::Result<std::unique_ptr<VehiclePropValue>>
UserHal::onSetCreateUserResponse(
    const VehiclePropValue& value) {
    if (value.value.int32Values.size() == 0) {
        ALOGE("set(CREATE_USER): no int32values, ignoring it: %s",
              toString(value).c_str());
        return android::base::Error(INVALID_ARG)
               << "no int32values on " << toString(value);
    }

    ALOGD("set(CREATE_USER) called from Android: %s", toString(value).c_str());

    int32_t requestId = value.value.int32Values[0];

    auto updatedValue = createVehiclePropValue(VehiclePropertyType::INT32, 2);
    updatedValue->prop = CREATE_USER;
    updatedValue->timestamp = value.timestamp;
    updatedValue->status = value.status;
    updatedValue->value.int32Values[0] = requestId;
    updatedValue->value.int32Values[1] = static_cast<int32_t>(CreateUserStatus::SUCCESS);

    return updatedValue;
}

android::base::Result<std::unique_ptr<VehiclePropValue>>
UserHal::onSetUserIdentificationAssociation(const VehiclePropValue& value) {
    if (value.value.int32Values.size() == 0) {
        ALOGE("set(USER_IDENTIFICATION_ASSOCIATION): no int32values, ignoring it: %s",
              toString(value).c_str());
        return android::base::Error(INVALID_ARG)
               << "no int32values on " << toString(value);
    }

    ALOGI("set(USER_IDENTIFICATION_ASSOCIATION) called from Android: %s",
          toString(value).c_str());
    auto updatedValue = getListAssociationTypes(value, requestType::SET);

    if (updatedValue == nullptr) {
        return android::base::Error(INVALID_ARG) << "Bad arguments";
    }
    // Returns default response
    return defaultUserIdentificationAssociation(*updatedValue.get());
}

android::base::Result<std::unique_ptr<VehiclePropValue>>
UserHal::defaultUserIdentificationAssociation(const VehiclePropValue& request) {
    int32_t requestId = request.value.int32Values[0];
    int32_t numTypesQueried = request.value.int32Values[1];
    auto response = createVehiclePropValue(VehiclePropertyType::INT32,
                                           numTypesQueried * 2 + 2);

    response->prop = USER_IDENTIFICATION_ASSOCIATION;
    response->status = request.status;
    response->timestamp = request.timestamp;
    response->value.int32Values[0] = requestId;
    response->value.int32Values[1] = numTypesQueried;

    for (size_t i = 2, j = 2; j < request.value.int32Values.size(); i += 2, ++j) {
        response->value.int32Values[i] = request.value.int32Values[j];
        response->value.int32Values[i + 1] = NOT_ASSOCIATED_ANY_USER;
    }

    //return a response with NOT_ASSOCIATED_ANY_USER for all requested types
    return response;
}

}  // namespace renesas

}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
