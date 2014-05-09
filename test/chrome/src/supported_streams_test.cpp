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
#define LOG_TAG "HAL_supported_streams_test"

#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include "test_parameterization.h"
#include "test_utils.h"
#include "test_stream_factory.h"

using namespace android;
namespace Parmz = Parameterization;
namespace TSF = TestStreamFactory;

extern camera_module_t *HAL_MODULE_INFO_SYM_PTR;  // For 'HMI'

class SupportedStreamsTest : public ::testing::TestWithParam<Parmz::TestParam> {};

TEST_P(SupportedStreamsTest, TestStream)
{
    Parmz::TestParam param = GetParam();

    // Check that we don't try to parameterize a camera that is not on the HW
    // otherwise we'll crash at find_camera_metadata_ro_entry()
    ASSERT_LT(param.cameraId, HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras());

    // get the static metadata, which has available stream configs
    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(param.cameraId, &ac2info);
    const camera_metadata_t *meta = ac2info.static_camera_characteristics;

    ASSERT_NE(meta, nullptr);

    camera_metadata_ro_entry_t entry;
    entry.count = 0;
    int ret = find_camera_metadata_ro_entry(meta,
                             ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                             &entry);

    ASSERT_EQ(ret, OK);

    int count = entry.count;
    const int32_t *availStreamConfig = entry.data.i32;

    ASSERT_NE(availStreamConfig, nullptr);
    ASSERT_NE(true, (count < 4));

    PRINTLN("Testing camera %d supports resolution %dx%d format %d(0x%x)",
            param.cameraId,
            param.width,
            param.height,
            param.format,
            param.format);

    bool found = false;
    // find tested config from list of supported
    for (uint32_t j = 0; j < (uint32_t)count; j += 4) {
        if (availStreamConfig[j] == param.format &&
            availStreamConfig[j + 1] == param.width &&
            availStreamConfig[j + 2] == param.height &&
            availStreamConfig[j + 3] == CAMERA3_STREAM_OUTPUT) {
            found = true;
            break;
        }
    }

    ASSERT_EQ(found, true);
}

// Supported streams test for sensors for camera ID = 0
// TODO: Add more tests to cover given sensor configuration
INSTANTIATE_TEST_CASE_P(camera0,
                        SupportedStreamsTest,
                        ::testing::ValuesIn(Parmz::getSupportedStreams(&TSF::getSupportedStreams, 0)));

INSTANTIATE_TEST_CASE_P(camera1,
                        SupportedStreamsTest,
                        ::testing::ValuesIn(Parmz::getSupportedStreams(&TSF::getSupportedStreams, 1)));

