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

#define LOG_TAG "HAL_basic"

#include "raw_hal_test.h"

#include <string.h>
#include <stdint.h>
#include <memory>
#include <cstdio>
#include <math.h>
#include <semaphore.h>

using std::vector;
using namespace android;
namespace Parmz = Parameterization;
namespace TSF = TestStreamFactory;

extern int gFrameCount;

void pickMaxResolutionSize(int cameraid, uint32_t format, int &width, int &height)
{
    int numCameras = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();
    if (numCameras < cameraid+1) {
        PRINTLN("The select camera is not available, skipping the test.");
        return;
    }

    // get the static metadata, which has available stream configs
    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(cameraid, &ac2info);
    const camera_metadata_t *meta = ac2info.static_camera_characteristics;

    if (meta == nullptr) {
        PRINTLN("Test startup issue - no metadata available!");
        return;
    }

    camera_metadata_ro_entry_t entry;
    entry.count = 0;
    int ret = find_camera_metadata_ro_entry(meta,
                             ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                             &entry);

    if (ret != OK) {
        PRINTLN("Test startup issue - no stream configurations");
        return;
    }

    int count = entry.count;
    const int32_t *availStreamConfig = entry.data.i32;
    if (count < 4 || availStreamConfig == nullptr) {
        PRINTLN("Test startup issue - not enough valid stream configurations");
        return;
    }

    // find configs to test
    for (uint32_t j = 0; j < (uint32_t)count; j += 4) {
        if (availStreamConfig[j] == format &&
            availStreamConfig[j+3] == CAMERA3_STREAM_OUTPUT) {
            if (width * height < availStreamConfig[j + 1] * availStreamConfig[j + 2]) {
                width = availStreamConfig[j + 1];
                height = availStreamConfig[j + 2];
            }
        }
    }
}

/**
 * Create a hardcoded stream configuration and pass it to HAL
 */
TEST_P(RawHal_Test, TestStreamConfigHardcoded)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, 1920, 1080);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;
}

/**
 * Issue a single stream capture request and dump it to file
 */
TEST_P(RawHal_Test, TestSingleFrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;
    camera3_stream_buffer streamBuffer;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, 1280, 720);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(streams[0].width,
                             streams[0].height,
                             streams[0].format,
                             streams[0].usage,
                             streamPtrs[0],
                             &streamBuffer);
    ASSERT_EQ(status, 0) << "allocate Buffer failed: status" \
                         << std::hex <<  status;
    request.frame_number = 0;
    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;
    request.output_buffers = &streamBuffer;

    mDumpAfterFrame = 0;
    std::unique_lock<std::mutex> lock(mTestMutex);
    status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
    ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                         << std::hex <<  status;

    // wait max 5 seconds
    int ret = mTestCondition.waitRelative(lock, 5 * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
    ASSERT_EQ(ret, 0) << "Request did not complete in 5 seconds";

    free_camera_metadata(requestSettings);
}

/**
 * Issue a single stream capture request and dump it to file
 */
TEST_P(RawHal_Test, TestSingle1080pFrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;
    camera3_stream_buffer streamBuffer;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(streams[0].width,
                             streams[0].height,
                             streams[0].format,
                             streams[0].usage,
                             streamPtrs[0],
                             &streamBuffer);
    ASSERT_EQ(status, 0) << "allocate Buffer failed: status" \
                         << std::hex <<  status;
    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;
    request.output_buffers = &streamBuffer;

    std::unique_lock<std::mutex> lock(mTestMutex);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int frameCount = gFrameCount;
    mDumpAfterFrame = gDumpEveryFrame ? 0 : (frameCount - 1);
    for (int i = 0; i < frameCount; i++) {
        request.frame_number = i;
        status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
        ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                             << std::hex <<  status;

        // wait max 1 seconds
        int ret = mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
        ASSERT_EQ(ret, 0) << "Request did not complete in 1 seconds";
    }

    gettimeofday(&end, NULL);

    int64_t microseconds = (end.tv_sec - start.tv_sec) * 1000000LL +
                           (end.tv_usec - start.tv_usec);
    float fps = frameCount * 1000000.0f / microseconds;
    PRINTLN("1080p test ran with %f fps", fps);

    free_camera_metadata(requestSettings);
}

/**
 * Issue 20 single stream capture requests and dump last to file
 */
TEST_P(RawHal_Test, Test20FrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;
    camera3_stream_buffer streamBuffer;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(streams[0].width,
                             streams[0].height,
                             streams[0].format,
                             streams[0].usage,
                             streamPtrs[0],
                             &streamBuffer);
    ASSERT_EQ(status, 0) << "allocate Buffer failed: status" \
                         << std::hex <<  status;
    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;
    request.output_buffers = &streamBuffer;

    std::unique_lock<std::mutex> lock(mTestMutex);
    int frameCount = 20;
    mDumpAfterFrame = gDumpEveryFrame ? 0 : (frameCount - 1);
    for (int i = 0; i < frameCount; i++) {
        request.frame_number = i;
        status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
        ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                             << std::hex <<  status;

        // wait max 1 seconds
        int ret = mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
        ASSERT_EQ(ret, 0) << "Request did not complete in 1 seconds";
    }

    free_camera_metadata(requestSettings);
}

/**
 * Issue 100 single stream capture requests and dump fps info
 */
TEST_P(RawHal_Test, Test100FrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;
    camera3_stream_buffer streamBuffer;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(streams[0].width,
                             streams[0].height,
                             streams[0].format,
                             streams[0].usage,
                             streamPtrs[0],
                             &streamBuffer);
    ASSERT_EQ(status, 0) << "allocate Buffer failed: status" \
                         << std::hex <<  status;
    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;
    request.output_buffers = &streamBuffer;

    std::unique_lock<std::mutex> lock(mTestMutex);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int frameCount = 100;
    mDumpAfterFrame = gDumpEveryFrame ? 0 : frameCount;
    for (int i = 0; i < frameCount; i++) {
        request.frame_number = i;
        status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
        ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                             << std::hex <<  status;

        // wait max 1 seconds
        int ret = mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
        ASSERT_EQ(ret, 0) << "Request did not complete in 1 seconds";
    }
    gettimeofday(&end, NULL);

    int64_t microseconds = (end.tv_sec - start.tv_sec) * 1000000LL +
                           (end.tv_usec - start.tv_usec);
    float fps = frameCount * 1000000.0f / microseconds;
    PRINTLN("1080p test ran with %f fps", fps);

    free_camera_metadata(requestSettings);
}

/**
 * Issue 100000 single stream capture requests and dump fps info
 */
TEST_P(RawHal_Test, Test100000FrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;
    camera3_stream_buffer streamBuffer;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(streams[0].width,
                             streams[0].height,
                             streams[0].format,
                             streams[0].usage,
                             streamPtrs[0],
                             &streamBuffer);
    ASSERT_EQ(status, 0) << "allocate Buffer failed: status" \
                         << std::hex <<  status;
    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;
    request.output_buffers = &streamBuffer;

    std::unique_lock<std::mutex> lock(mTestMutex);
    struct timeval start, end;
    gettimeofday(&start, NULL);
    int frameCount = 100000;
    mDumpAfterFrame = gDumpEveryFrame ? 0 : frameCount;
    for (int i = 0; i < frameCount; i++) {
        request.frame_number = i;
        status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
        ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                             << std::hex <<  status;

        // wait max 1 seconds
        int ret = mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
        ASSERT_EQ(ret, 0) << "Request did not complete in 1 seconds";
    }
    gettimeofday(&end, NULL);

    int64_t microseconds = (end.tv_sec - start.tv_sec) * 1000000LL +
                           (end.tv_usec - start.tv_usec);
    float fps = frameCount * 1000000.0f / microseconds;
    PRINTLN("1080p test ran with %f fps", fps);

    free_camera_metadata(requestSettings);
}

 /*
 * 2 streams with each 20 frames capture requests and dump last to file
 */
TEST_P(RawHal_Test, Test2Streams720pResolutions)
{
    Parmz::MultiStreamsTestParam params;
    params.params[0] = Parmz::TestParam(mCameraId, 1280, 720, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
    params.params[1] = Parmz::TestParam(mCameraId, 1280, 720, HAL_PIXEL_FORMAT_YCbCr_420_888);
    process2StreamsRequests(FRAMES_FOR_MULTI_STREAMS, params);
}

/**
 * Issue single stream capture requests with multiple different buffers
 * and dump some of them to file - at least last
 */
TEST_P(RawHal_Test, TestMultiBufferFrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    int frameCount = 5;
    status = allocateBuffers(frameCount, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    // with only 5 buffers and frames, HAL won't probably block at all
    processMultiBufferRequests(frameCount, request);

    // wait that requests are finished
    waitFramesToComplete(frameCount);

    free_camera_metadata(requestSettings);
}

/**
 * Issue 500 single stream capture requests with multiple different buffers
 * and dump some of them to file - at least last
 */
TEST_P(RawHal_Test, Test500MultiBufferFrameCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // This test is used for dual camera test, so set the target fps to 30.
    // Otherwise the cameras will run at default variable fps, which
    // causes the test to fail.
    CameraMetadata meta(requestSettings); // takes metadata ownership
    setFps(30, meta);
    requestSettings = meta.release(); // restore metadata ownership (new ptr)

    // Allocate memory
    status = allocateBuffers(8, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    int frameCount = 500;
    mDumpAfterFrame = gDumpEveryFrame ? 0 : (frameCount - 1);

    PRINTLN("processMultiBufferRequests, camera id: %d, frame count: %d", mCameraId, frameCount);
    processMultiBufferRequests(frameCount, request);

    // wait that requests are finished
    PRINTLN("waitFramesToComplete, camera id: %d, frame count: %d", mCameraId, frameCount);
    waitFramesToComplete(frameCount);

    free_camera_metadata(requestSettings);
}

/**
 * Issue 200 single stream capture requests with multiple different buffers
 * using 4K resolution, if available, and measure fps
 */
TEST_P(RawHal_Test, Test4KSpeed)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    // figure out if 4K is supported
    bool isSupported = false;
    is4KSupported(isSupported);

    if (!isSupported) {
        PRINTLN("4K resolution is not available in stream configs. "
                "skipping the test.");
        return;
    }

    int width = 3840;
    int height = 2160;

    PRINTLN("Testing YUV stream config for resolution %dx%d", width, height);

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, width, height);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Set target fps to 30
    CameraMetadata meta(requestSettings); // takes metadata ownership
    setFps(30, meta);
    requestSettings = meta.release(); // restore metadata ownership (new ptr)

    // Allocate memory
    status = allocateBuffers(8, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    int frameCount = 400;
    struct timeval start, end;
    gettimeofday(&start, NULL);
    processMultiBufferRequests(frameCount, request);

    // wait that requests are finished
    waitFramesToComplete(frameCount);
    gettimeofday(&end, NULL);
    int64_t microseconds = (end.tv_sec - start.tv_sec) * 1000000LL +
                           (end.tv_usec - start.tv_usec);
    float fps = frameCount * 1000000.0f / microseconds;
    PRINTLN("4K speed test ran with %f fps", fps);

    free_camera_metadata(requestSettings);
}


/**
 * Issue single stream capture requests with multiple different buffers and
 * wait for 3A convergence. Max 400 frames before failing if no convergence.
 */
TEST_P(RawHal_Test, Test3AConvergence)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(8, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    int frameCount = 400;
    mDumpAfterFrame = gDumpEveryFrame ? 0 : (frameCount - 1);
    processMultiBufferRequests(frameCount, request, true);

    // wait that requests are finished and FAIL the test if there is no
    // convergence
    wait3AToConverge(mRequestsIssued, true);

    free_camera_metadata(requestSettings);
}

/**
 * Issue single stream capture requests with multiple different buffers and
 * wait for 3A convergence. Max 400 frames before failing if no convergence.
 * Issue AE bracketing captures and dump them to file with ev compensation
 * values of -1, 0 and +1.
 *
 * This test relies on reads being thread-safe and locks sparsely
 *
 */
TEST_P(RawHal_Test, TestAEBracketing)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    // figure out ev step size
    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
    ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);
    // check that there are some static metadata in it
    CameraMetadata staticMeta;
    staticMeta = ac2info.static_camera_characteristics;
    camera_metadata_entry evStepSizeEntry = staticMeta.find(ANDROID_CONTROL_AE_COMPENSATION_STEP);
    ASSERT_EQ(evStepSizeEntry.count, 1);

    status_t err = createSingleStreamConfig(streamConfig, streams[0], streamPtrs, mTestWidth, mTestHeight);
    ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                      << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // Allocate memory
    status = allocateBuffers(8, &streams[0]);
    ASSERT_EQ(status, OK) << "Buffer allocation failed";

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    // run the camera for a while, until 3A corverges or max 400 frames
    int frameCount = 400;
    processMultiBufferRequests(frameCount, request, true);

    // wait that requests are finished and 3A is converged
    wait3AToConverge(mRequestsIssued);
    ASSERT_NE(mConvergedIso, 0);
    ASSERT_NE(mConvergedExposureTime, 0);

    // create 3 requests with different exposures via EV shifts
    CameraMetadata halfExposure;
    // ev -1
    int32_t ev = round(-1.0f * evStepSizeEntry.data.r[0].denominator / evStepSizeEntry.data.r[0].numerator);
    halfExposure = requestSettings;
    halfExposure.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &ev, 1);

    CameraMetadata normalExposure(halfExposure); // clone
    // ev 0
    ev = 0;
    normalExposure.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &ev, 1);

    CameraMetadata doubleExposure(halfExposure); // clone
    // ev +1
    ev = round(1.0f * evStepSizeEntry.data.r[0].denominator / evStepSizeEntry.data.r[0].numerator);
    doubleExposure.update(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION, &ev, 1);

    vector<camera_metadata_t *> bracketingSettings;
    bracketingSettings.push_back(halfExposure.release());
    bracketingSettings.push_back(normalExposure.release());
    bracketingSettings.push_back(doubleExposure.release());

    // process and dump(true) the bracketing
    processBracketingRequests(request, bracketingSettings, true);

    // a qualitative test of the result data could be here. For the time being,
    // one must verify the dumped three frames manually as NV12 images.

    free_camera_metadata(requestSettings);
    for (const auto &setting : bracketingSettings) {
        free_camera_metadata(setting);
    }
}

/**
 * List of auto-exposure modes supported by this camera device
 *
 */
TEST_P(RawHal_Test, TestAEAvailableModes)
{
    uint32_t status = 0;

    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
    ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);

    // check that there are some static metadata in it
    const camera_metadata_t *staticMeta = ac2info.static_camera_characteristics;

    if (staticMeta == nullptr) {
        PRINTLN("Test startup issue - no metadata available!");
        return;
    }

    camera_metadata_ro_entry_t availablemodes;
    availablemodes.count = 0;
    int ret = find_camera_metadata_ro_entry(staticMeta,
            ANDROID_CONTROL_AE_AVAILABLE_MODES,
            &availablemodes);
    if (ret != OK) {
                PRINTLN("No available AE modes");
                return;
    }

    PRINTLN("Get %d available AE modes", availablemodes.count);
    const uint8_t *modes = availablemodes.data.u8;
    for (int index = 0; index < availablemodes.count; index++) {
        char modestring[100];
        ret = camera_metadata_enum_snprint(ANDROID_CONTROL_AE_MODE, modes[index], modestring, 100);
        PRINTLN("\t%d. %s", index, modestring);
    }
}

/**
 * List of auto-focus modes supported by this camera device
 *
 */
TEST_P(RawHal_Test, TestAFAvailableModes)
{
    uint32_t status = 0;

    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
    ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);

    // check that there are some static metadata in it
    const camera_metadata_t *staticMeta = ac2info.static_camera_characteristics;

    if (staticMeta == nullptr) {
        PRINTLN("Test startup issue - no metadata available!");
        return;
    }

    camera_metadata_ro_entry_t availablemodes;
    availablemodes.count = 0;
    int ret = find_camera_metadata_ro_entry(staticMeta,
            ANDROID_CONTROL_AF_AVAILABLE_MODES,
            &availablemodes);
    if (ret != OK) {
                PRINTLN("No available AF modes");
                return;
    }

    PRINTLN("Get %d available AF modes", availablemodes.count);
    const uint8_t *modes = availablemodes.data.u8;
    for (int index = 0; index < availablemodes.count; index++) {
        char modestring[100];
        ret = camera_metadata_enum_snprint(ANDROID_CONTROL_AF_MODE, modes[index], modestring, 100);
        PRINTLN("\t%d. %s", index, modestring);
    }
}

/**
 * List of auto-white-balance modes supported by this camera device
 *
 */
TEST_P(RawHal_Test, TestAWBAvailableModes)
{
    uint32_t status = 0;

    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
    ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);

    // check that there are some static metadata in it
    const camera_metadata_t *staticMeta = ac2info.static_camera_characteristics;

    if (staticMeta == nullptr) {
        PRINTLN("Test startup issue - no metadata available!");
        return;
    }

    camera_metadata_ro_entry_t availablemodes;
    availablemodes.count = 0;
    int ret = find_camera_metadata_ro_entry(staticMeta,
            ANDROID_CONTROL_AWB_AVAILABLE_MODES,
            &availablemodes);
    if (ret != OK) {
                PRINTLN("No available AWB modes");
                return;
    }

    PRINTLN("Get %d available AWB modes", availablemodes.count);
    const uint8_t *modes = availablemodes.data.u8;
    for (int index = 0; index < availablemodes.count; index++) {
        char modestring[100];
        ret = camera_metadata_enum_snprint(ANDROID_CONTROL_AWB_MODE, modes[index], modestring, 100);
        PRINTLN("\t%d. %s", index, modestring);
    }
}

/**
 * List of scene modes supported by this camera device
 *
 */
TEST_P(RawHal_Test, TestAvailableSecneModes)
{
    uint32_t status = 0;

    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
    ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);

    // check that there are some static metadata in it
    const camera_metadata_t *staticMeta = ac2info.static_camera_characteristics;

    if (staticMeta == nullptr) {
        PRINTLN("Test startup issue - no metadata available!");
        return;
    }

    camera_metadata_ro_entry_t availablemodes;
    availablemodes.count = 0;
    int ret = find_camera_metadata_ro_entry(staticMeta,
            ANDROID_CONTROL_AVAILABLE_SCENE_MODES,
            &availablemodes);
    if (ret != OK) {
                PRINTLN("No available scene modes");
                return;
    }

    PRINTLN("Get %d available scene modes", availablemodes.count);
    const uint8_t *modes = availablemodes.data.u8;
    for (int index = 0; index < availablemodes.count; index++) {
        char modestring[100];
        ret = camera_metadata_enum_snprint(ANDROID_CONTROL_SCENE_MODE, modes[index], modestring, 100);
        PRINTLN("\t%d. %s", index, modestring);
    }
}

/**
 * List of live effects supported by this camera device
 *
 */
TEST_P(RawHal_Test, TestAvailableLiveEffects)
{
    uint32_t status = 0;

    struct camera_info ac2info;
    HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
    ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);

    // check that there are some static metadata in it
    const camera_metadata_t *staticMeta = ac2info.static_camera_characteristics;

    if (staticMeta == nullptr) {
        PRINTLN("Test startup issue - no metadata available!");
        return;
    }

    camera_metadata_ro_entry_t availablemodes;
    availablemodes.count = 0;
    int ret = find_camera_metadata_ro_entry(staticMeta,
            ANDROID_CONTROL_AVAILABLE_EFFECTS,
            &availablemodes);
    if (ret != OK) {
                PRINTLN("No available effect modes");
                return;
    }

    PRINTLN("Get %d available effect modes", availablemodes.count);
    const uint8_t *modes = availablemodes.data.u8;
    for (int index = 0; index < availablemodes.count; index++) {
        char modestring[100];
        ret = camera_metadata_enum_snprint(ANDROID_CONTROL_EFFECT_MODE, modes[index], modestring, 100);
        PRINTLN("\t%d. %s", index, modestring);
    }
}

/**
 * Config 720P single stream resolution with multiple different buffers
 * and capture one frame
 *
 * This test is left here only as a reference. Parametrised tests are
 * implemented and they include this resolution if it is supported.
 */
TEST_P(RawHal_Test, Test720PResolution)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    // every sensor should support this resolution so no need to really check
    // static metadata. For now, it is a bug, if this is not supported.
    int width = 1280;
    int height = 720;

    PRINTLN("Testing YUV stream config for resolution %dx%d", width, height);

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

INSTANTIATE_TEST_CASE_P(basic,
                        RawHal_Test,
                        ::testing::ValuesIn(Parmz::getCameraValues()));
