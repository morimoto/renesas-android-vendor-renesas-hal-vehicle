# Copyright (C) 2017 The Android Open Source Project
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

LOCAL_PATH := $(call my-dir)

########################################################################
# Vehicle HAL service

include $(CLEAR_VARS)
LOCAL_MODULE := android.hardware.automotive.vehicle@2.0-service.renesas
LOCAL_INIT_RC := android.hardware.automotive.vehicle@2.0-service.renesas.rc
LOCAL_PROPRIETARY_MODULE := true
LOCAL_MODULE_RELATIVE_PATH := hw

LOCAL_SRC_FILES := \
    VehicleService.cpp \
    VehicleHalImpl.cpp

LOCAL_SHARED_LIBRARIES := \
    libbase \
    libhidlbase \
    libhidltransport \
    libhwbinder \
    liblog \
    libutils \
    android.hardware.automotive.vehicle@2.0

LOCAL_STATIC_LIBRARIES := \
    android.hardware.automotive.vehicle@2.0-manager-lib

include $(BUILD_EXECUTABLE)
