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

package vehicle_hal_defaults

import (
    "android/soong/android"
    "android/soong/cc"
)

func globalFlags(ctx android.BaseContext) []string {
    var cflags []string

    if ctx.AConfig().Getenv("TARGET_PRODUCT") == "salvator" {
        cflags = append(cflags, "-DTARGET_PRODUCT_SALVATOR=1")
    } else if ctx.AConfig().Getenv("TARGET_PRODUCT") == "kingfisher" {
        cflags = append(cflags, "-DTARGET_PRODUCT_KINGFISHER=1")
    }

    return cflags
}

func myDefaults(ctx android.LoadHookContext) {
    type props struct {
        Cflags []string
    }

    p := &props{}
    p.Cflags = globalFlags(ctx)

    ctx.AppendProperties(p)
}

func init() {
    android.RegisterModuleType("vehicle_hal_defaults", VehicleHalDefaultsFactory)
}

func VehicleHalDefaultsFactory() android.Module {
    module := cc.DefaultsFactory()
    android.AddLoadHook(module, myDefaults)

    return module
}
