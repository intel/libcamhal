/*
 * Copyright (C) 2016-2017 Intel Corporation
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
#define LOG_TAG "HAL_yuv"

#include "raw_hal_test.h"

class RawHal_Test_Yuv_Resolutions : public RawHal_Test {};

class RawHal_Test_YUV_with_Config : public ::testing::TestWithParam< std::tr1::tuple<Parmz::TestParam, Parmz::MetadataTestParam> > {};

/**
 * Config YUV single stream resolution with multiple different buffers
 * and capture one frame.
 */
TEST_P(RawHal_Test_Yuv_Resolutions, TestYuv)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    Parmz::TestParam param = GetParam();
    int width = param.width;
    int height = param.height;

    PRINTLN("Testing camera %d YUV stream config for resolution %dx%d",
            param.cameraId,
            width,
            height);

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, width, height);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    // run with preview template to allow AF to retry
    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(2, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    runSingleStreamCapturesAndDump(request, requestSettings);

    free_camera_metadata(requestSettings);
}

TEST_P(RawHal_Test_Yuv_Resolutions, TestYuvNo3a)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    Parmz::TestParam param = GetParam();
    int width = param.width;
    int height = param.height;

    PRINTLN("Testing camera %d YUV stream config for resolution %dx%d",
            param.cameraId,
            width,
            height);

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, width, height);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_STILL_CAPTURE);

    // Allocate memory
    status = allocateBuffers(2, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    runSingleStreamCapturesAndDump(request, requestSettings, false /*no 3a wait*/);

    free_camera_metadata(requestSettings);
}


TEST_P(RawHal_Test_YUV_with_Config, YuvCaptureWithConfig)
{
    status_t status = OK;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    Parmz::TestParam param =  std::tr1::get<0>(GetParam());
    Parmz::MetadataTestParam metadata =  std::tr1::get<1>(GetParam());
    int yuvWidth = 0;
    int yuvHeight = 0;

    pickMaxResolutionSize(param.cameraId, HAL_PIXEL_FORMAT_YCbCr_420_888, yuvWidth, yuvHeight);
    if (yuvWidth == 0 || yuvHeight == 0)
        return;

    // limit yuv resolution to max 1920x1080 as test with 4K is not stable
    if (yuvWidth*yuvHeight > 1920*1080) {
        yuvWidth = 1920;
        yuvHeight = 1080;
    }

    param.width = yuvWidth;
    param.height = yuvHeight;

    // select test to run with the camera
    RawHal_Test_Yuv_Resolutions_TestYuv_Test test;
    test.silencePrint = true;

    // set the camera id parameter for the test manually
    test.SetTestParam(&param);

    // set the camera metadata configure
    test.SetCameraConfigure(metadata.tag, metadata.value);
    char modestring[100];
    camera_metadata_enum_snprint(metadata.tag, metadata.value, modestring, 100);
    PRINTLN("YuvCaptureWithConfig: metadata tag %s, mode %s", get_camera_metadata_tag_name(metadata.tag), modestring);

    // since we use a fixture class we need to set it up manually
    test.SetUp();

    // run the test
    test.TestBody();

    // tear down the test
    test.TearDown();
}

INSTANTIATE_TEST_CASE_P(yuv_resolutions,
                        RawHal_Test_Yuv_Resolutions,
                        ::testing::ValuesIn(Parmz::getResolutionValues(HAL_PIXEL_FORMAT_YCbCr_420_888)));


INSTANTIATE_TEST_CASE_P(CameraSettings_YUV,
                        RawHal_Test_YUV_with_Config,
                        ::testing::Combine(::testing::ValuesIn(Parmz::getCameraValues()), ::testing::ValuesIn(Parmz::getMetadataTestEntries())));
