/*
 * Copyright (C) 2019 GlobalLogic
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

#include <iomanip>
#include <fstream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <errno.h>
#include <utils/SystemClock.h>
#include <log/log.h>
#include <android-base/macros.h>

#include "VehicleHalImpl.h"
#include "DefaultConfig.h"

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {
namespace renesas {

#define SIZEOF_BIT_ARRAY(bits)  ((bits + 7) / 8)
#define TEST_BIT(bit, array)    (array[bit / 8] & (1 << (bit % 8)))

typedef struct __attribute__((packed, aligned(2))) vhal_can_msg_s {
    int32_t     propId;
    int32_t     propValue;
} vhal_can_msg_t;

using StateReq = VehicleApPowerStateReq;
using Shutdown = VehicleApPowerStateShutdownParam;

VehicleHalImpl::VehicleHalImpl(VehiclePropertyStore* propStore, UserHal* userHal) :
    mUserHal(userHal),
    mPropStore(propStore),
    mHvacPowerProps(std::begin(kHvacPowerProperties), std::end(kHvacPowerProperties)),
    mRecurrentTimer(std::bind(&VehicleHalImpl::onContinuousPropertyTimer,
                                  this, std::placeholders::_1)),
    mSocket(socket(PF_CAN, SOCK_RAW, CAN_RAW)),
    mCanThreadExit(false),
    mGpioThreadExit(false)
{
    for (size_t i = 0; i < arraysize(kVehicleProperties); i++) {
        mPropStore->registerProperty(kVehicleProperties[i].config);
    }

    if (mSocket < 0) {
        ALOGE("CAN RAW socket is NOT created. Vehicle HAL will be offline.");
    }

    std::memset(&mSockAddr, 0, sizeof(mSockAddr));
}

VehicleHalImpl::~VehicleHalImpl(void)
{
    ALOGD("%s: ->", __func__);

    mCanThreadExit = true;  // Notify thread to finish and wait for it to terminate.
    mGpioThreadExit = true; //

    if (mCanThread.joinable()) {
        mCanThread.join();
    }
    if (mGpioThread.joinable()) {
        mGpioThread.join();
    }

    if (mSocket != -1) {
        close(mSocket);
    }

    ALOGD("%s: <-", __func__);
}

void VehicleHalImpl::onCreate(void)
{
    for (auto& it : kVehicleProperties) {
        VehiclePropConfig cfg = it.config;
        int32_t numAreas = cfg.areaConfigs.size();

//        if (isDiagnosticProperty(cfg)) {
//             do not write an initial empty value for the diagnostic properties
//             as we will initialize those separately.
//            continue;
//        }

        //  A global property will have supportedAreas = 0
        if (isGlobalProp(cfg.prop)) {
            numAreas = 1;
        }

        // This loop is a do-while so it executes at least once to handle global properties
        for (int i = 0; i < numAreas; i++) {
            int32_t curArea;

            if (isGlobalProp(cfg.prop)) {
                curArea = 0;
            } else {
                curArea = cfg.areaConfigs[i].areaId;
            }

            // Create a separate instance for each individual zone
            VehiclePropValue prop = {
                .prop = cfg.prop,
                .areaId = curArea,
            };
            if (it.initialAreaValues.size() > 0) {
                auto valueForAreaIt = it.initialAreaValues.find(curArea);
                if (valueForAreaIt != it.initialAreaValues.end()) {
                    prop.value = valueForAreaIt->second;
                } else {
                    ALOGW("%s failed to get default value for prop 0x%x area 0x%x",
                            __func__, cfg.prop, curArea);
                }
            } else {
                prop.value = it.initialValue;
            }
            mPropStore->writeValue(prop, true);

        }
    }

    if (mSocket != -1) {
        struct ifreq ifr;
        std::memset(&ifr.ifr_name, 0, sizeof(ifr.ifr_name));
        std::strcpy(ifr.ifr_name, "can0");

        if (ioctl(mSocket, SIOCGIFINDEX, &ifr) < 0) {
            ALOGE("ioctl SIOCGIFINDEX failed (error %d)", errno);
        } else {
            mSockAddr.can_family = AF_CAN;
            mSockAddr.can_ifindex = ifr.ifr_ifindex;

            if (bind(mSocket, (struct sockaddr*)&mSockAddr, sizeof(mSockAddr)) < 0) {
                ALOGE("bind CAN socket failed (error %d)", errno);
                close(mSocket);
                mSocket = -1;
            } else {
                ALOGI("CAN RAW: IFACE=%s, IFINDEX=%d, SOCKET=%d\n", ifr.ifr_name, ifr.ifr_ifindex, mSocket);
            }
        }
    }

    mCanThread = std::thread(&VehicleHalImpl::CanRxHandleThread, this);
    mGpioThread = std::thread(&VehicleHalImpl::GpioHandleThread, this);
}

std::vector<VehiclePropConfig> VehicleHalImpl::listProperties(void)
{
    return mPropStore->getAllConfigs();
}

VehicleHal::VehiclePropValuePtr
VehicleHalImpl::get(const VehiclePropValue& requestedPropValue, StatusCode* outStatus)
{
    ALOGD("..get PropValue: %s", toString(requestedPropValue).c_str());
    VehiclePropValuePtr propValuePtr = {nullptr};

    if (mUserHal->isSupported(static_cast<int32_t>(requestedPropValue.prop))) {
        auto ret = mUserHal->onGetProperty(requestedPropValue);

        if (!ret.ok()) {
            ALOGE("onGetProperty(): HAL returned error: %s", ret.error().message().c_str());
            *outStatus = StatusCode(ret.error().code());
        } else if (ret.value() != nullptr) {
            auto response = ret.value().get();

            ALOGI("onGetProperty(): property returned by HAL: %s",
                  toString(*response).c_str());
            propValuePtr = getValuePool()->obtain(*response);
        }
    } else {
        auto internalPropValue = mPropStore->readValueOrNull(requestedPropValue);

        if (internalPropValue != nullptr) {
            propValuePtr = getValuePool()->obtain(*internalPropValue);
        }
    }

    ALOGV("..get 0x%08x", requestedPropValue.prop);

    *outStatus = (propValuePtr != nullptr) ? StatusCode::OK : StatusCode::INVALID_ARG;
    return propValuePtr;
}

StatusCode VehicleHalImpl::updatePropValue(const VehiclePropValue& propValueIn,
                                           VehiclePropValue& propValueOut,
                                           bool& isUpdated)
{
    if (mUserHal->isSupported(static_cast<int32_t>(propValueIn.prop))) {
        auto ret = mUserHal->onSetProperty(propValueIn);
        if (!ret.ok()) {
            ALOGE("onSetProperty(): HAL returned error: %s", ret.error().message().c_str());
            return StatusCode(ret.error().code());
        }

        if (ret.value() != nullptr) {
            propValueOut = *(ret.value().get());
            isUpdated = true;
            ALOGI("onSetProperty(): updating property returned by HAL: %s",
                  toString(propValueOut).c_str());
        }
    }

    return StatusCode::OK;
}

void VehicleHalImpl::sendCanMsg(const VehiclePropValue& propValue)
{
    vhal_can_msg_t msg = {propValue.prop, 0};

    if (propValue.value.int32Values.size() != 0) {
        msg.propValue = static_cast<int32_t>(propValue.value.int32Values[0]);
    } else if (propValue.value.floatValues.size() != 0) {
        msg.propValue = (int32_t)propValue.value.floatValues[0];
    } else if (propValue.value.int64Values.size() != 0) {
        ALOGW("TODO: INT64 values send is unsupported by now");
    } else if(propValue.value.bytes.size() != 0) {
        ALOGW("TODO: BYTE-array send is unsupported by now");
    }

    VehicleHalImpl::CanTxBytes(&msg, sizeof(msg));
}

StatusCode VehicleHalImpl::set(const VehiclePropValue& propValue)
{
    ALOGD("..set PropValue: %s", toString(propValue).c_str());
    VehiclePropValue updatedPropValue = propValue;
    bool isUpdated = false;

    if (mHvacPowerProps.count(propValue.prop)) {
        auto hvacPowerOn = mPropStore->readValueOrNull(toInt(VehicleProperty::HVAC_POWER_ON),
                                                       toInt(VehicleAreaSeat::ROW_1_CENTER));

        if (hvacPowerOn && hvacPowerOn->value.int32Values.size() == 1
                && hvacPowerOn->value.int32Values[0] == 0) {
            return StatusCode::NOT_AVAILABLE;
        }
    }

    if (auto ret = updatePropValue(propValue, updatedPropValue, isUpdated);
                                   ret != StatusCode::OK){
        return ret;
    }

    if (!mPropStore->writeValue(updatedPropValue, true)) {
        ALOGW("write value error, propValue: %s", toString(updatedPropValue).c_str());
        return StatusCode::INVALID_ARG;
    }

    // respond back to Android if it needed
    if (isUpdated) {
        if (getValuePool() != NULL) {
            doHalEvent(getValuePool()->obtain(updatedPropValue));
        } else {
            ALOGW("getValuePool() == NULL: propId: 0x%x", updatedPropValue.prop);
        }
    }

    sendCanMsg(updatedPropValue);
    return StatusCode::OK;
}

StatusCode VehicleHalImpl::subscribe(int32_t property, float sampleRate)
{
    ALOGI("%s propId: 0x%x, sampleRate: %f", __func__, property, sampleRate);

    if (isContinuousProperty(property)) {
        mRecurrentTimer.registerRecurrentEvent(hertzToNanoseconds(sampleRate), property);
    }
    return StatusCode::OK;
}

StatusCode VehicleHalImpl::unsubscribe(int32_t property)
{
    ALOGI("%s propId: 0x%x", __func__, property);
    if (isContinuousProperty(property)) {
        mRecurrentTimer.unregisterRecurrentEvent(property);
    }
    return StatusCode::OK;
}

void VehicleHalImpl::onContinuousPropertyTimer(const std::vector<int32_t>& properties)
{
    VehiclePropValuePtr propValuePtr;
    auto& pool = *getValuePool();

    for (int32_t property : properties) {
        if (isContinuousProperty(property)) {
            auto internalPropValue = mPropStore->readValueOrNull(property);
            if (internalPropValue != nullptr) {
                propValuePtr = pool.obtain(*internalPropValue);
            }
        } else {
            ALOGE("Unexpected onContinuousPropertyTimer for property: 0x%x", property);
        }

        if (propValuePtr.get()) {
            propValuePtr->timestamp = elapsedRealtimeNano();
            doHalEvent(std::move(propValuePtr));
        }
    }
}

void VehicleHalImpl::onGpioStateChanged(int fd, unsigned char* const key_bitmask, size_t array_len)
{
    VehiclePropValue propValue = {
        .prop = toInt(VehicleProperty::GEAR_SELECTION),
        .areaId = toInt(VehicleArea::GLOBAL),
        .timestamp = elapsedRealtimeNano(),
        .value.int32Values = {toInt(VehicleGear::GEAR_NEUTRAL)}
    };

    if (ioctl(fd, EVIOCGKEY(array_len), key_bitmask) >= 0) {
        if (TEST_BIT(KEY_F4, key_bitmask)) { /* SW2 - 4 */
            propValue.value.int32Values[0] = toInt(VehicleGear::GEAR_REVERSE);
            ALOGI("Curreant gear: REVERSE");
        } else if (TEST_BIT(KEY_F3, key_bitmask)) { /* SW2 - 3 */
            propValue.value.int32Values[0] = toInt(VehicleGear::GEAR_PARK);
            ALOGI("Curreant gear: PARKING");
        } else {
            ALOGI("Curreant gear: NEUTRAL");
        }

        if (this->set(propValue) == StatusCode::OK) {
            if (getValuePool() != NULL) {
                doHalEvent(getValuePool()->obtain(propValue));
            } else {
                ALOGW("getValuePool() == NULL: propId: 0x%x", propValue.prop);
            }
        }
    }
}

bool VehicleHalImpl::isContinuousProperty(int32_t propId) const
{
    const VehiclePropConfig* config = mPropStore->getConfigOrNull(propId);
    if (config == nullptr) {
        ALOGW("Config not found for property: 0x%x", propId);
        return false;
    }
    return config->changeMode == VehiclePropertyChangeMode::CONTINUOUS;
}

void VehicleHalImpl::CanRxHandleThread(void)
{
    if (mCanThreadExit || mSocket == -1) {
        return;
    }

    fd_set rdfs;
    char ctrlmsg[CMSG_SPACE(sizeof(struct timeval)) + CMSG_SPACE(sizeof(__u32))];
    struct can_frame frame = {
        .can_dlc = CAN_MAX_DLEN
    };
    struct iovec iov = {
        .iov_base = &frame
    };
    struct msghdr sock_msg = {
        .msg_name = &mSockAddr,
        .msg_iov = &iov,
        .msg_iovlen = 1,
        .msg_control = &ctrlmsg
    };
    ALOGD("CanRxHandleThread() ->");

    while (!mCanThreadExit) {
        FD_ZERO(&rdfs);
        FD_SET(mSocket, &rdfs);

        if (select(mSocket+1, &rdfs, NULL, NULL, NULL) <= 0) {
            ALOGI("SELECT failed, errno %d ...", errno);
            break;
        }

        if (FD_ISSET(mSocket, &rdfs)) {
            iov.iov_len = sizeof(frame);

            sock_msg.msg_namelen = sizeof(mSockAddr);
            sock_msg.msg_controllen = sizeof(ctrlmsg);
            sock_msg.msg_flags = 0;

            int bytes = recvmsg(mSocket, &sock_msg, 0);

            if (bytes < 0) {
                if ((errno == ENETDOWN) && !mCanThreadExit) {
                    ALOGE("CAN interface is down");
                    continue;
                }

                ALOGE("CAN socket read error %d", bytes);
                break;
            }

            vhal_can_msg_t* pmsg = reinterpret_cast<vhal_can_msg_t*>(&frame.data);

            ALOGD("RX: prop = 0x%08x, val = 0x%08x", pmsg->propId, pmsg->propValue);

            std::vector<VehiclePropValue> propValues = mPropStore->readAllValues();
            for (size_t i = 0; i < propValues.size(); i++) {
                VehiclePropValue &propValue = propValues[i];
                if (propValue.prop == pmsg->propId) {
                    if (propValue.value.int32Values.size() != 0) {
                        if (pmsg->propId == (int32_t)VehicleProperty::AP_POWER_STATE_REQ) {
                            propValue.value.int32Values.resize(2);
                            propValue.value.int32Values[0] = static_cast<int32_t>(pmsg->propValue & 0xFFFF);
                            propValue.value.int32Values[1] = static_cast<int32_t>(pmsg->propValue >> 16);
                            if constexpr (c_strcmp( TARGET_PRODUCT, salvator) == 0) {
                                if (propValue.value.int32Values[0] == static_cast<int32_t>(
                                    StateReq::SHUTDOWN_PREPARE) &&
                                    propValue.value.int32Values[1] == static_cast<int32_t>(
                                    Shutdown::CAN_SLEEP)) {
                                    setPmicBackupMode(BackupMode::ON);
                                } else if (propValue.value.int32Values[0] == static_cast<int32_t>(
                                    StateReq::CANCEL_SHUTDOWN)) {
                                    setPmicBackupMode(BackupMode::OFF);
                                }
                            }
                        } else {
                            propValue.value.int32Values[0] = static_cast<int32_t>(pmsg->propValue);
                        }
                    } else if (propValue.value.floatValues.size() != 0){
                        propValue.value.floatValues[0] = (float)pmsg->propValue;
                    } else if (propValue.value.int64Values.size() != 0){
                        ALOGW("TODO: INT64 values receive is unsupported by now");
                    } else if(propValue.value.bytes.size() != 0){
                        ALOGW("TODO: BYTE-array send is unsupported by now");
                    }

                    propValue.timestamp = elapsedRealtimeNano();

                    if (mPropStore->writeValue(propValue, true)) {
                        if (getValuePool() != NULL) {
                            doHalEvent(getValuePool()->obtain(propValue));
                        } else {
                            ALOGW("getValuePool() == NULL: propId: 0x%x", propValue.prop);
                        }
                    }
                    break;
                }
            }
        }
    }

    ALOGD("CanRxHandleThread() <-");
}

void VehicleHalImpl::CanTxBytes(void* bytesPtr, size_t bytesCount)
{
    if (mSocket == -1) {
        return;
    }

    struct can_frame frame = {
        .can_dlc = CAN_MAX_DLEN
    };

    if (frame.can_dlc > bytesCount){
        frame.can_dlc = bytesCount;
    }

    std::memcpy(&frame.data, bytesPtr, frame.can_dlc);

    if (send(mSocket, &frame, CAN_MTU, 0) < 0 ) {
        ALOGE("Send %d bytes failed, error %d", frame.can_dlc, errno);
    } else {
        ALOGD("CAN sent %d bytes", frame.can_dlc);
    }
}

void VehicleHalImpl::GpioHandleThread(void)
{
    if (mGpioThreadExit) {
        return;
    }

    ALOGD("GpioHandleThread() ->");

    constexpr size_t maxRetry {12};
    std::chrono::milliseconds timeout {1};
    int fd = -1;
    for (size_t i = 0; i < maxRetry && fd < 0 && !mGpioThreadExit; ++i) {
        fd = open("/dev/input/event0", O_RDONLY);
        if (fd < 0) {
            ALOGW("Could not open input event device, attempt %zu, error: %s.", i, strerror(errno));
            std::this_thread::sleep_for(timeout);
            timeout *= 2;
        }
    }
    if (fd < 0) {
        ALOGE("Could not open input event device, after %zu retry. Exit from GPIO thread.", maxRetry);
        return;
    }

    unsigned char key_bitmask[SIZEOF_BIT_ARRAY(KEY_MAX + 1)];
    std::memset(key_bitmask, 0, sizeof(key_bitmask));

    struct pollfd fds = {
        .fd = fd,
        .events = POLLIN,
        .revents = POLLOUT
    };

    /**
     * Here is we check initial GPIO switches state
     * in case we want to boot up straight into
     * the EVS app.
     */
    onGpioStateChanged(fd, key_bitmask, sizeof(key_bitmask));

    while (!mGpioThreadExit) {
        if (poll(&fds, 1, -1) > 0) {
            if (read(fds.fd, key_bitmask, sizeof(key_bitmask)) > 0) {
                onGpioStateChanged(fd, key_bitmask, sizeof(key_bitmask));
            }
        }
    }

    close(fd);

    ALOGD("GpioHandleThread() <-");
}

void VehicleHalImpl::setPmicBackupMode(BackupMode mode) const
{
    std::ofstream backupModeStream;
    backupModeStream.open(mBackupModeFileName, std::ofstream::out);
    if (backupModeStream.is_open()) {
        backupModeStream << BackupModeToStr(mode) << std::endl;
        backupModeStream.close();
        ALOGD("PMIC Backup Mode : %s", BackupModeToStr(mode));
    } else {
        ALOGE("PMIC configuration failed!");
    }
}

}  // namespace renesas
}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android
