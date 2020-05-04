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

#ifndef _VehicleHal_H_
#define _VehicleHal_H_

#include <vector>
#include <thread>
#include <unordered_set>

#include <inttypes.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <net/if.h>
#include <memory.h>
#include <poll.h>

#include <linux/can.h>
#include <linux/can/raw.h>
#include <linux/input.h>
#include <linux/input-event-codes.h>

#include <vhal_v2_0/RecurrentTimer.h>
#include <vhal_v2_0/VehicleHal.h>
#include <vhal_v2_0/VehiclePropertyStore.h>

constexpr const char* salvator = "salvator";
constexpr const char* kingfisher = "kingfisher";

constexpr int c_strcmp( char const* lhs, char const* rhs )
{
    return (('\0' == lhs[0]) && ('\0' == rhs[0])) ? 0
        :  (lhs[0] != rhs[0]) ? (lhs[0] - rhs[0])
        : c_strcmp( lhs+1, rhs+1 );
}

namespace android {
namespace hardware {
namespace automotive {
namespace vehicle {
namespace V2_0 {
namespace renesas {

class VehicleHalImpl : public VehicleHal {
public:
    VehicleHalImpl(VehiclePropertyStore* propStore);
    virtual ~VehicleHalImpl(void);

    virtual std::vector<VehiclePropConfig> listProperties() override;
    virtual VehicleHal::VehiclePropValuePtr get(const VehiclePropValue& requestedPropValue,
                                        StatusCode* outStatus) override;
    virtual StatusCode set(const VehiclePropValue& propValue) override;
    virtual StatusCode subscribe(int32_t property, float sampleRate) override;
    virtual StatusCode unsubscribe(int32_t property) override;
    virtual void onCreate() override;

    void GpioHandleThread(void);
    void CanRxHandleThread(void);
    void CanTxBytes(void* bytesPtr, size_t bytesCount);

private:
    constexpr std::chrono::nanoseconds hertzToNanoseconds(float hz) const {
        return std::chrono::nanoseconds(static_cast<int64_t>(1000000000L / hz));
    }

    void onGpioStateChanged(int fd, unsigned char* const key_bitmask, size_t array_len);
    void onContinuousPropertyTimer(const std::vector<int32_t>& properties);
    bool isContinuousProperty(int32_t propId) const;

    enum class BackupMode { ON, OFF};
    constexpr const char* BackupModeToStr(BackupMode mode) const {
        switch (mode) {
            case BackupMode::ON:
                return "on";
            case BackupMode::OFF:
                return "off";
        }
    }
    void setPmicBackupMode(BackupMode mode) const;
    const std::string mBackupModeFileName = "/sys/bus/platform/devices/bd9571mwv-regulator/backup_mode";

    VehiclePropertyStore*           mPropStore;
    std::unordered_set<int32_t>     mHvacPowerProps;
    RecurrentTimer                  mRecurrentTimer;
    int                             mSocket;
    struct sockaddr_can             mSockAddr;
    std::thread                     mCanThread;
    std::atomic<bool>               mCanThreadExit;
    std::thread                     mGpioThread;
    std::atomic<bool>               mGpioThreadExit;
};

}  // namespace renesas
}  // namespace V2_0
}  // namespace vehicle
}  // namespace automotive
}  // namespace hardware
}  // namespace android

#endif // _VehicleHal_H_
