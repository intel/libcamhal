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
#define LOG_TAG "HAL_multi_streams_test"
#include "raw_hal_test.h"

namespace TSF = TestStreamFactory;
namespace Parmz = Parameterization;
class Multi_Streams_Test : public ::Basic_Test, public ::testing::WithParamInterface<Parmz::MultiStreamsTestParam> {
public:
    const Parmz::MultiStreamsTestParam& GetParam() const
    {
       return ::testing::WithParamInterface<Parmz::MultiStreamsTestParam>::GetParam();
    }

    void SetUp()
    {
        Parmz::MultiStreamsTestParam params = GetParam();
        mCameraId = params.params[0].cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        Basic_Test::TearDown();
    }
};

/**
 * Multi streams with params, each single frame capture
 * and dump last frame to file.
 */
TEST_P(Multi_Streams_Test, TestMultiStreams)
{
    Parmz::MultiStreamsTestParam params = GetParam();
    process2StreamsRequests(FRAMES_FOR_MULTI_STREAMS, params);
}

class Multi_Streams_Test_Fix_Params : public ::Basic_Test, public ::testing::WithParamInterface<Parmz::MultiStreamsTestParam> {
public:
    const Parmz::MultiStreamsTestParam& GetParam() const
    {
       return ::testing::WithParamInterface<Parmz::MultiStreamsTestParam>::GetParam();
    }

    void SetUp()
    {
        Parmz::MultiStreamsTestParam params = GetParam();
        mCameraId = params.params[0].cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        Basic_Test::TearDown();
    }
};

/**
 * Multi streams with fix params, each single frame capture
 * and dump last frame to file.
 */
TEST_P(Multi_Streams_Test_Fix_Params, TestMultiStreams)
{
    Parmz::MultiStreamsTestParam params = GetParam();
    process2StreamsRequests(FRAMES_FOR_MULTI_STREAMS, params);
}

class Jpeg_Test : public ::Basic_Test, public ::testing::WithParamInterface<Parmz::MultiStreamsTestParam> {
public:

    const Parmz::MultiStreamsTestParam& GetParam() const
    {
       return ::testing::WithParamInterface<Parmz::MultiStreamsTestParam>::GetParam();
    }

    void SetUp()
    {
        Parmz::MultiStreamsTestParam params = GetParam();
        mCameraId = params.params[0].cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        Basic_Test::TearDown();
    }
};

class LiveShot_Test : public ::Basic_Test, public ::testing::WithParamInterface<Parmz::MultiStreamsTestParam> {
public:

    const Parmz::MultiStreamsTestParam& GetParam() const
    {
       return ::testing::WithParamInterface<Parmz::MultiStreamsTestParam>::GetParam();
    }

    void SetUp()
    {
        Parmz::MultiStreamsTestParam params = GetParam();
        mCameraId = params.params[0].cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        Basic_Test::TearDown();
    }
};


TEST_P(LiveShot_Test, TestJpegDuringVideo)
{
    Parmz::MultiStreamsTestParam params = GetParam();
    process3StreamsBlobRequests(params);
}

/**
 * Config preview + jpeg streams and issue preview capture requests with
 * multiple different buffers and wait for 3A convergence.
 * Max 10 frames before failing if no convergence. After convergence, issue
 * JPEG capture and dump it to filesystem.
 */
TEST_P(Jpeg_Test, TestJpegWithPrevCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[2];
    camera3_stream_t *streamPtrs[2];
    camera3_capture_request_t request;

    Parmz::MultiStreamsTestParam params = GetParam();
    int prevWidth = params.params[1].width;
    int prevHeight = params.params[1].height;
    int jpegWidth = params.params[0].width;
    int jpegHeight = params.params[0].height;

    PRINTLN("Configuring camera %d preview + jpeg streams.", params.params[0].cameraId);
    PRINTLN("JPEG resolution %dx%d", jpegWidth, jpegHeight);
    PRINTLN("Preview resolution %dx%d", prevWidth, prevHeight);

    createJpegStreamConfig(streamConfig,
                           streams[0],  // preview
                           streams[1],  // jpeg
                           streamPtrs,
                           prevWidth, prevHeight,
                           jpegWidth, jpegHeight);

    // run with preview template to allow AF to retry
    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(4, &streams[0], &mBuffers);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    // Allocate memory
    status = allocateBuffers(2, &streams[1], &mJpegBuffers);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    PRINTLN("Running preview until 3A converges.");
    int frameCount = 400;
    processMultiBufferRequests(frameCount, request, true);

    // wait that requests are finished
    wait3AToConverge(mRequestsIssued);

    // swap to use the still capture settings
    free_camera_metadata(requestSettings);
    requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_STILL_CAPTURE);

    request.num_output_buffers = 2;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    //we also dump the last preview image
    //mJpegFrameNumber = mRequestsIssued + 1;  // next frame is the jpeg

    frameCount = 1;
    mDumpAfterFrame = 0; // dump the jpeg
    mTestStreams = 2;
    processJpegRequests(frameCount, request);

    // wait that requests are finished
    waitFramesToComplete(mRequestsIssued + 1);

    free_camera_metadata(requestSettings);
}

class Camera_Streams_Test : public ::Basic_Test, public ::testing::WithParamInterface<Parmz::MultiStreamsTestParam> {
public:

    const Parmz::MultiStreamsTestParam& GetParam() const
    {
       return ::testing::WithParamInterface<Parmz::MultiStreamsTestParam>::GetParam();
    }

    void SetUp()
    {
        Parmz::MultiStreamsTestParam params = GetParam();
        mCameraId = params.params[0].cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        Basic_Test::TearDown();
    }
};

TEST_P(Camera_Streams_Test, TestCameraStreams)
{
    Parmz::MultiStreamsTestParam params = GetParam();
    process2StreamsRequests(FRAMES_FOR_MULTI_STREAMS, params);
}

// Multi streams test for sensors
INSTANTIATE_TEST_CASE_P(multi_streams,
                        Multi_Streams_Test,
                        ::testing::ValuesIn(Parmz::getMultiResolutionValues(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, HAL_PIXEL_FORMAT_YCbCr_420_888)));

// Multi streams test by fix params for camera ID = 1
// TODO: Add more tests to cover given sensor configuration
INSTANTIATE_TEST_CASE_P(camera1,
                        Multi_Streams_Test_Fix_Params,
                        ::testing::ValuesIn(TSF::getMultiStreamsTestParams(1)));

INSTANTIATE_TEST_CASE_P(camera0,
                        Multi_Streams_Test_Fix_Params,
                        ::testing::ValuesIn(TSF::getMultiStreamsTestParams(0)));

INSTANTIATE_TEST_CASE_P(camera1,
                        Jpeg_Test,
                        ::testing::ValuesIn(TSF::getJpegTestParams(1)));

INSTANTIATE_TEST_CASE_P(camera0,
                        Jpeg_Test,
                       ::testing::ValuesIn(TSF::getJpegTestParams(0)));

INSTANTIATE_TEST_CASE_P(camera1,
                       LiveShot_Test,
                       ::testing::ValuesIn(TSF::getTripleStreamsTestParams(1)));

INSTANTIATE_TEST_CASE_P(camera0,
                       LiveShot_Test,
                      ::testing::ValuesIn(TSF::getTripleStreamsTestParams(0)));


INSTANTIATE_TEST_CASE_P(Camera0,
                       Camera_Streams_Test,
                       ::testing::ValuesIn(TSF::getCameraStreamsTestParams(0)));

INSTANTIATE_TEST_CASE_P(Camera1,
                       Camera_Streams_Test,
                       ::testing::ValuesIn(TSF::getCameraStreamsTestParams(1)));
