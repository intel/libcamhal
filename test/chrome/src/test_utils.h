/*
 * Copyright (C) 2016-2018 Intel Corporation
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

#ifndef IPU4_UNITTESTS_SRC_TEST_UTILS_H_
#define IPU4_UNITTESTS_SRC_TEST_UTILS_H_

#include <gtest/gtest.h>
#include <dlfcn.h>
#include <hardware/camera3.h>

#define CAMERA_HAL_LIB "/usr/lib64/camera_hal.so"

#define CDEV(dev) ((camera3_device_t *) dev)
#define DOPS(dev) (CDEV(dev)->ops)
#define DCOMMON(dev) (CDEV(dev)->common)

namespace t_i = testing::internal;

/*
 * Error codes.
 * All error codes are negative values.
 */

enum {
    OK                  = 0,    // Everything's swell.

    UNKNOWN_ERROR       = (-2147483647-1), // INT32_MIN value

    NO_MEMORY           = -ENOMEM,
    INVALID_OPERATION   = -ENOSYS,
    BAD_VALUE           = -EINVAL,
    BAD_TYPE            = (UNKNOWN_ERROR + 1),
    NAME_NOT_FOUND      = -ENOENT,
    PERMISSION_DENIED   = -EPERM,
    NO_INIT             = -ENODEV,
    ALREADY_EXISTS      = -EEXIST,
    DEAD_OBJECT         = -EPIPE,
    FAILED_TRANSACTION  = (UNKNOWN_ERROR + 2),
    JPARKS_BROKE_IT     = -EPIPE,
#if !defined(HAVE_MS_C_RUNTIME)
    BAD_INDEX           = -EOVERFLOW,
    NOT_ENOUGH_DATA     = -ENODATA,
    WOULD_BLOCK         = -EWOULDBLOCK,
    TIMED_OUT           = -ETIMEDOUT,
    UNKNOWN_TRANSACTION = -EBADMSG,
#else
    BAD_INDEX           = -E2BIG,
    NOT_ENOUGH_DATA     = (UNKNOWN_ERROR + 3),
    WOULD_BLOCK         = (UNKNOWN_ERROR + 4),
    TIMED_OUT           = (UNKNOWN_ERROR + 5),
    UNKNOWN_TRANSACTION = (UNKNOWN_ERROR + 6),
#endif
    FDS_NOT_ALLOWED     = (UNKNOWN_ERROR + 7),
    NO_ENTRY            = (UNKNOWN_ERROR + 8),
};

namespace testing {
namespace internal {
enum GTestColor {
    COLOR_DEFAULT, COLOR_RED, COLOR_GREEN, COLOR_YELLOW
};

extern void ColoredPrintf(GTestColor color, const char* fmt, ...);
} // namespace internal
} // namespace testing

#define PRINTF(...) \
do { \
    t_i::ColoredPrintf(t_i::COLOR_GREEN, "[          ] "); \
    t_i::ColoredPrintf(t_i::COLOR_YELLOW, __VA_ARGS__); \
} while(0)

#define PRINTLN(...) \
do { \
    PRINTF(__VA_ARGS__); \
    t_i::ColoredPrintf(t_i::COLOR_YELLOW, "\n"); \
} while(0)

#define BAIL(exittag, progress) \
    do { \
        /* add these, as needed */ \
        if (progress == 0) goto exittag##0; \
        if (progress == 1) goto exittag##1; \
        if (progress == 2) goto exittag##2; \
        ASSERT_TRUE(progress < 3); \
    } while (0)

static camera_module_t *getHalModuleInfo()
{
    void *handleOfHAL = dlopen(CAMERA_HAL_LIB, RTLD_LAZY);
    if(handleOfHAL == nullptr) {
        PRINTLN("dlopen HAL library failed: %s ", dlerror());
        return nullptr;
    }

    camera_module_t *halModuleInfoPtr = (camera_module_t *)dlsym(handleOfHAL, "HMI");
    if(halModuleInfoPtr == nullptr) {
        PRINTLN("dlsym handleOfHAL failed: %s ", dlerror());
        return nullptr;
    }

    halModuleInfoPtr->common.dso = handleOfHAL;
    return halModuleInfoPtr;
}

static int releaseHalModuleSo(void *handleOfHAL)
{
    if(handleOfHAL == nullptr) {
        PRINTLN("handleOfHAL is invalid");
        return -EINVAL;
    }

    int ret = dlclose(handleOfHAL);
    if(ret != 0) {
        PRINTLN("dlclose handleOfHAL failed");
        return -EIO;
    }

    handleOfHAL = nullptr;
    return 0;
}

#endif /* IPU4_UNITTESTS_SRC_TEST_UTILS_H_ */
