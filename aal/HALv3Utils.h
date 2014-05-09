/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#ifndef _HALV3_UTILS_
#define _HALV3_UTILS_

#include "iutils/CameraLog.h"

namespace camera3 {

#undef LOG1
#undef LOG2
#undef LOGI
#undef LOGW
#undef LOGE

#ifdef HAVE_LINUX_OS

#define LOG1(format, args...) \
    icamera::Log::print_log(icamera::gLogLevel & icamera::CAMERA_DEBUG_LOG_LEVEL1,\
            LOG_TAG, icamera::CAMERA_DEBUG_LOG_LEVEL1, format, ##args)
#define LOG2(format, args...) \
    icamera::Log::print_log(icamera::gLogLevel & icamera::CAMERA_DEBUG_LOG_LEVEL2,\
            LOG_TAG, icamera::CAMERA_DEBUG_LOG_LEVEL2, format, ##args)
#define LOGI(format, args...) \
    icamera::Log::print_log(true, LOG_TAG, icamera::CAMERA_DEBUG_LOG_INFO, format, ##args)
#define LOGW(format, args...) \
    icamera::Log::print_log(true, LOG_TAG, icamera::CAMERA_DEBUG_LOG_WARNING, format, ##args)
#define LOGE(format, args...) \
    icamera::Log::print_log(true, LOG_TAG, icamera::CAMERA_DEBUG_LOG_ERR, format, ##args)

#else

#define LOG1(...) icamera::__camera_hal_log(icamera::gLogLevel & icamera::CAMERA_DEBUG_LOG_LEVEL1,\
                        ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOG2(...) icamera::__camera_hal_log(icamera::gLogLevel & icamera::CAMERA_DEBUG_LOG_LEVEL2,\
                        ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) icamera::__camera_hal_log(true, ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGW(...) icamera::__camera_hal_log(true, ANDROID_LOG_WARN, LOG_TAG, __VA_ARGS__)
#define LOGE(...) icamera::__camera_hal_log(true, ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)

#endif

} // namespace camera3
#endif // _HALV3_UTILS_
