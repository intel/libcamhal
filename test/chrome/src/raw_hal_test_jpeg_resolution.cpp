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

#define LOG_TAG "HAL_jpeg"

#include "raw_hal_test.h"
#include <math.h>

namespace Parmz = Parameterization;

class RawHal_Test_Jpeg_Resolutions : public RawHal_Test {};
class RawHal_Test_Jpeg_Resolutions_Focus_Infinity : public RawHal_Test {};
class RawHal_Test_Jpeg_with_Config : public ::testing::TestWithParam< std::tr1::tuple<Parmz::TestParam, Parmz::MetadataTestParam> > {};

// Helper function declarations.
void pickPreferredPreviewSize(const int &jpegWidth, const int &jpegHeight, int &width, int &height);


/**
 * Config preview + jpeg streams and issue preview capture requests with
 * multiple different buffers and wait for 3A convergence.
 * Max 400 frames before failing if no convergence. After convergence, issue
 * JPEG capture and dump it to filesystem.
 */
TEST_P(RawHal_Test_Jpeg_Resolutions, TestJpegCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[2];
    camera3_stream_t *streamPtrs[2];
    camera3_capture_request_t request;

    Parmz::TestParam param = GetParam();
    int jpegWidth = param.width;
    int jpegHeight = param.height;

    // Default values, real ones determined below
    int prevWidth = 640;
    int prevHeight = 480;
    pickPreferredPreviewSize(jpegWidth, jpegHeight, prevWidth, prevHeight);

    PRINTLN("Configuring camera %d preview + jpeg streams.", param.cameraId);
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

    mJpegFrameNumber = mRequestsIssued + 1;  // next frame is the jpeg

    frameCount = 1;
    mDumpAfterFrame = 0; // dump the jpeg
    processJpegRequests(frameCount, request);

    // wait that requests are finished
    waitFramesToComplete(mRequestsIssued);

    free_camera_metadata(requestSettings);
}

/**
 * Config preview + jpeg streams and issue preview capture requests with
 * multiple different buffers and wait for 3A convergence. Focus to infinity.
 * Max 400 frames before failing if no convergence.
 */
TEST_P(RawHal_Test_Jpeg_Resolutions_Focus_Infinity, TestJpegCapture)
{
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[2];
    camera3_stream_t *streamPtrs[2];
    camera3_capture_request_t request;

    Parmz::TestParam param = GetParam();
    int jpegWidth = param.width;
    int jpegHeight = param.height;

    if (!isManualFocusSupported(param.cameraId)) {
        PRINTLN("Manual focus is not supported. Skipping the test.");
        return;
    }

    // Default values, real ones determined below
    int prevWidth = 640;
    int prevHeight = 480;
    pickPreferredPreviewSize(jpegWidth, jpegHeight, prevWidth, prevHeight);

    PRINTLN("Configuring camera %d preview + jpeg streams.", param.cameraId);
    PRINTLN("JPEG resolution %dx%d", jpegWidth, jpegHeight);
    PRINTLN("Preview resolution %dx%d", prevWidth, prevHeight);

    createJpegStreamConfig(streamConfig,
                           streams[0],  // preview
                           streams[1],  // jpeg
                           streamPtrs,
                           prevWidth, prevHeight,
                           jpegWidth, jpegHeight);


    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_STILL_CAPTURE);

    // Set AF to off, focus to infinity
    CameraMetadata meta(requestSettings); // takes metadata ownership
    setManualFocus(meta, 0.0f);
    requestSettings = meta.release(); // restore metadata ownership (new ptr)

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

    // run for 10 more frames, allowing lens to move, just in case.
    // HAL should actually report the lens movement, but there are bugs in there
    frameCount = 10;
    processMultiBufferRequests(frameCount, request, false);
    // wait that requests are finished
    waitFramesToComplete(mRequestsIssued);

    request.num_output_buffers = 2;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    mJpegFrameNumber = mRequestsIssued + 1;  // next frame is the jpeg

    frameCount = 1;
    mDumpAfterFrame = 0; // dump the jpeg
    processJpegRequests(frameCount, request);

    // wait that requests are finished
    waitFramesToComplete(mRequestsIssued);

    free_camera_metadata(requestSettings);
}


TEST_P(RawHal_Test_Jpeg_Resolutions, TestSingleStreamJpegCapture)
{
    status_t status = OK;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    Parmz::TestParam param = GetParam();
    int jpegWidth = param.width;
    int jpegHeight = param.height;

    //extend frame count to 10 to leave more time to camera configure
    int frameCount = 10;

    PRINTLN("Configuring camera %d jpeg stream.", param.cameraId);
    PRINTLN("JPEG resolution %dx%d", jpegWidth, jpegHeight);
    PRINTLN("Number of frames %d", frameCount);

    status = createSingleStreamJpegStreamConfig(streamConfig,
                                                streams[0],  // jpeg
                                                streamPtrs,
                                                jpegWidth, jpegHeight);
    ASSERT_EQ(status, OK) << "createSingleStreamJpegStreamConfig failed with status " << status;


    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_STILL_CAPTURE);

    // Allocate memory
    status = allocateBuffers(6, &streams[0], &mJpegBuffers);
    ASSERT_EQ(status, OK) << "Buffer allocation failed with status " << status;

    request.num_output_buffers = 1;
    request.input_buffer = NULL;
    request.settings = requestSettings;

    mJpegFrameNumber = frameCount + 1;  // next frame is the jpeg

    mDumpAfterFrame = gDumpEveryFrame ? 0 : (frameCount - 1); // dump the jpeg
    status = processSingleStreamJpegRequests(frameCount, request);
    ASSERT_EQ(status, OK) << "processSingleStreamJpegRequests failed with status " << status;

    // wait that requests are finished
    waitFramesToComplete(mRequestsIssued);

    free_camera_metadata(requestSettings);
}

void pickPreferredPreviewSize(const int &jpegWidth, const int &jpegHeight, int &prevWidth, int &prevHeight)
{
    // Assume user responsibility for picking a good preview size
    if (jpegHeight == 0 || jpegWidth == 0)
        return;

    // Get supported preview sizes.
    // NOTE: use same format as in RawHal_Test::create*StreamConfig()
    std::vector<Parmz::TestParam> previewSizes
        = Parmz::getResolutionValues(HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);

    // Cap at biggest _reasonable_ preview size. Currently 1920x1080.
    // Then select biggest available size, with same aspect ratio as the JPEG (blob).
    // TODO: change capping to JPEG size, if wanted.
    const int capWidth = 1920;
    const int capHeight = 1080;
    float jpegAspect = jpegWidth * 1.0f / jpegHeight;

    Parmz::ImageSizeDescSort compareImageSize;
    std::sort(previewSizes.begin(), previewSizes.end(), compareImageSize);

    // Remove sizes above the limit, based on image area.
    // Assumes a sorted vector, in descending order.
    std::vector<Parmz::TestParam>::const_iterator it = previewSizes.begin();
    while (it != previewSizes.end()) {
        if ((it->width * it->height) <= (capWidth * capHeight)) {
            previewSizes.erase(previewSizes.begin(), it);
            break;
        }

        ++it;
    }

    float previewAspect = 0.0f;
    const float ASPECT_TOLERANCE = 0.05f;

    // Pick the largest preview resolution matching JPEG.
    it = previewSizes.begin();
    while (it != previewSizes.end()) {
        previewAspect = static_cast<float>(it->width) / it->height;
        if (fabs(jpegAspect - previewAspect) < ASPECT_TOLERANCE)
            break;

        ++it;
    }

    if (it != previewSizes.end()) {
        // Didn't get to the end of the vector -> good size found
        prevWidth = it->width;
        prevHeight = it->height;
    } else {
        prevWidth = 640;
        prevHeight = 480;
    }
}

TEST_P(RawHal_Test_Jpeg_with_Config, JpegCaptureWithConfig)
{
    status_t status = OK;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;

    Parmz::TestParam param = std::tr1::get<0>(GetParam());
    Parmz::MetadataTestParam metadata = std::tr1::get<1>(GetParam());
    int jpegWidth = 0;
    int jpegHeight = 0;

    pickMaxResolutionSize(param.cameraId, HAL_PIXEL_FORMAT_BLOB, jpegWidth, jpegHeight);
    if (jpegHeight == 0 || jpegWidth == 0)
        return;

    param.width = jpegWidth;
    param.height = jpegHeight;

    // select test to run with the camera
    RawHal_Test_Jpeg_Resolutions_TestSingleStreamJpegCapture_Test test;
    test.silencePrint = true;

    // set the camera id parameter for the test manually
    test.SetTestParam(&param);

    // set the camera metadata configure
    test.SetCameraConfigure(metadata.tag, metadata.value);
    char modestring[100];
    camera_metadata_enum_snprint(metadata.tag, metadata.value, modestring, 100);
    PRINTLN("TestSingleStreamJpegCapture: metadata tag %s, mode %s", get_camera_metadata_tag_name(metadata.tag), modestring);

    // since we use a fixture class we need to set it up manually
    test.SetUp();

    // run the test
    test.TestBody();

    // tear down the test
    test.TearDown();
}

INSTANTIATE_TEST_CASE_P(jpeg_resolutions_inf,
                        RawHal_Test_Jpeg_Resolutions_Focus_Infinity,
                        ::testing::ValuesIn(Parmz::getResolutionValues(HAL_PIXEL_FORMAT_BLOB, true)));

INSTANTIATE_TEST_CASE_P(jpeg_resolutions,
                        RawHal_Test_Jpeg_Resolutions,
                        ::testing::ValuesIn(Parmz::getResolutionValues(HAL_PIXEL_FORMAT_BLOB)));

INSTANTIATE_TEST_CASE_P(CameraSettings_JPEG,
                        RawHal_Test_Jpeg_with_Config,
                        ::testing::Combine(::testing::ValuesIn(Parmz::getCameraValues()), ::testing::ValuesIn(Parmz::getMetadataTestEntries())));
