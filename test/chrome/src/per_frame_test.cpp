/*
 * Copyright (C) 2018 Intel Corporation
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
#define LOG_TAG "metadata_test"
#include "raw_hal_test.h"

namespace TSF = TestStreamFactory;
namespace Parmz = Parameterization;

#define MAX_STREAM_NUM 2
#define MAX_FRAME_NUM 10

#define COLOR_GAIN_LENGTH 4
#define TRANSFORM_LENGTH  9

enum settings_test_type {
    TEST_SENSOR_SETTINGS = 1 << 0,
    TEST_ISP_SETTINGS = 1 << 1
};

class Perframe_Test : public ::Basic_Test, public ::testing::WithParamInterface<Paramz::TestParam> {
public:
    Perframe_Test() :
    mParam(nullptr),
    mStatics(nullptr),
    mSettingsNum(0),
    mRequestCount(-1),
    mResultCount(-1) {}

    const Paramz::TestParam& GetParam() const
    {
        if (mParam) {
            return *mParam;
        } else {
            return ::testing::WithParamInterface<Paramz::TestParam>::GetParam();
        }
    }

    void SetUp()
    {
        Paramz::TestParam params = GetParam();
        mCameraId = params.cameraId;
        Basic_Test::SetUp();

        // get the static metadata, which has available stream configs
        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
        mStatics = ac2info.static_camera_characteristics;
    }

    void processCaptureResult(const camera3_capture_result_t *result)
    {
        PRINTLN(" Captured request %ld", result->frame_number);
        if (result && result->result) {
            mResultCount++;
            PRINTLN("[req] Get result %d", mResultCount);
            mResults[mResultCount % MAX_FRAME_NUM] = result->result;
        }

        Basic_Test::processCaptureResult(result);
    }

    void TearDown() {
        mParam = NULL;
        Basic_Test::TearDown();
    }

    void configureStreams(int streamNum = 1, int bufferNum = 1)
    {
        // Support one stream only now
        ASSERT_TRUE(streamNum == 1) << "Don't support streamNum: " << streamNum;

        camera3_stream_configuration_t streamConfig;
        camera3_stream_t *streamPtrs[1];
        camera3_capture_request_t request;

        // Configure streams
        status_t status = createSingleStreamConfig(streamConfig, mStreams[0], streamPtrs, 1920, 1080);
        ASSERT_EQ(status, 0) << "HAL stream config failed status: " \
                             << std::hex <<  status;

        // Allocate memory
        status = allocateBuffers(bufferNum, &mStreams[0]);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        if (!mSettingsNum) {
            camera_metadata_t* requestSettings =
                    constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
            mSettings[0].acquire(requestSettings);
            mSettingsNum = 1;
        }

        mTestStreams = streamNum;
        mRequestCount = -1;
        mResultCount = -1;
    }

    void sendRequests(int requestCount, bool perFrame = false)
    {
        camera3_capture_request_t request;
        std::unique_lock<std::mutex> lock(mTestMutex);

        for (int i = 0; i < requestCount; i++) {
            if (mBuffers.size() == 0) {
                mTestCondition.waitRelative(lock, 1.0f * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mBuffers.size() == 0) {
                    // TODO: asserts not good here in sub-functions
                    ASSERT_TRUE(false) << "timed out waiting for buffers";
                }
            }

            mRequestCount++;
            request.frame_number = mRequestCount;
            request.input_buffer = NULL;
            mStreamBuffer = mBuffers[0];
            mBuffers.erase(mBuffers.begin());
            request.output_buffers = &mStreamBuffer;
            request.num_output_buffers = 1;
            // Set settings for the 1st request or per-frame control
            int settingsIdx = -1;
            if (mRequestCount == 0 || perFrame) {
                settingsIdx = mRequestCount % MAX_FRAME_NUM;
                mSettings[settingsIdx].update(ANDROID_REQUEST_ID, &mRequestCount, 1);
                request.settings = mSettings[settingsIdx].getAndLock();
            } else {
                request.settings = nullptr;
            }

            mRequestsIssued++; // a bit premature, but it will assert if it fails
            PRINTLN("[req] Send request %d, handler %p, settings index %d",
                    mRequestCount, request.output_buffers->buffer, settingsIdx);
            status_t status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            PRINTLN("==done");
            ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                                 << std::hex <<  status;

            if (settingsIdx > 0) {
                mSettings[settingsIdx].unlock(request.settings);
            }
        }
    }

    void checkResults(int requestCount, int testTypes = 0)
    {
        waitMetaResultToComplete(requestCount);
        ASSERT_EQ(mRequestCount, mResultCount);

        for (int i = mRequestCount; i > (mRequestCount - requestCount); i--) {
            int index = i % MAX_FRAME_NUM;
            PRINTLN("%s: [req] Check req %d, index %d", __func__, i, index);
            dumpSensorSettings(index, mResults[index]);
            dumpIspSettings(index, mResults[index]);
            if (testTypes & TEST_SENSOR_SETTINGS) {
                checkSensorSettings(mSettings[index], mResults[index]);
            }
            if (testTypes & TEST_ISP_SETTINGS) {
                checkIspSettings(mSettings[index], mResults[index]);
            }
        }
    }

    void buildSensorSettings(int settingsNum) {
        if (settingsNum > MAX_FRAME_NUM)
            settingsNum = MAX_FRAME_NUM;

        int64_t exposureRange[2] = {1000, 2000000};
        camera_metadata_ro_entry_t entry;
        entry.count = 0;
        int ret = find_camera_metadata_ro_entry(mStatics,
                             ANDROID_SENSOR_INFO_EXPOSURE_TIME_RANGE,
                             &entry);
        ASSERT_EQ(ret, OK);
        exposureRange[0] = entry.data.i64[0];
        exposureRange[1] = entry.data.i64[1];

        int32_t sensitivityRange[2] = {100, 800};
        entry.count = 0;
        ret = find_camera_metadata_ro_entry(mStatics,
                             ANDROID_SENSOR_INFO_SENSITIVITY_RANGE,
                             &entry);
        ASSERT_EQ(ret, OK);
        sensitivityRange[0] = entry.data.i32[0];
        sensitivityRange[1] = entry.data.i32[1];

        // Use intermediate data of range
        const int64_t exposureStart = exposureRange[0] + (exposureRange[1] - exposureRange[0]) / 4;
        const int64_t exposureEnd = exposureRange[1] - (exposureRange[1] - exposureRange[0]) / 4;
        const int64_t exposureStep = (exposureEnd - exposureStart) / (settingsNum - 1);
        const int32_t gainStart = sensitivityRange[0] + (sensitivityRange[1] - sensitivityRange[0]) / 4;
        const int32_t gainEnd = sensitivityRange[1] - (sensitivityRange[1] - sensitivityRange[0]) / 4;
        const int32_t gainStep = (gainEnd - gainStart) / (settingsNum - 1);

        camera_metadata_t* requestSettings =
                constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
        CameraMetadata settings;
        settings.acquire(requestSettings);

        uint8_t mode = ANDROID_CONTROL_MODE_OFF;
        settings.update(ANDROID_CONTROL_MODE, &mode, 1);
        mode = ANDROID_CONTROL_AE_MODE_OFF;
        settings.update(ANDROID_CONTROL_AE_MODE, &mode, 1);
        for (int i = 0; i < settingsNum; i ++) {
            int64_t exposure = exposureStart + exposureStep * i;
            settings.update(ANDROID_SENSOR_EXPOSURE_TIME, &exposure, 1);
            int32_t gain = gainStart + gainStep * i;
            settings.update(ANDROID_SENSOR_SENSITIVITY, &gain, 1);
            mSettings[i] = settings;
            PRINTLN("%s: frame %d, exposure %ld, gain %d", __func__, i, exposure, gain);
        }

        mSettingsNum = settingsNum;
    }

    void checkSensorSettings(const CameraMetadata &settings, const CameraMetadata &result) {
        const float delta = 0.02; // %

        camera_metadata_ro_entry_t sEntry = settings.find(ANDROID_SENSOR_EXPOSURE_TIME);
        camera_metadata_ro_entry_t rEntry = result.find(ANDROID_SENSOR_EXPOSURE_TIME);
        ASSERT_EQ(sEntry.count, 1);
        ASSERT_EQ(rEntry.count, 1);
        ASSERT_TRUE(abs((float)rEntry.data.i64[0] / sEntry.data.i64[0]- 1.0) <= delta)
                << "set exposure " << sEntry.data.i64[0]
                << ", results " << rEntry.data.i64[0];

        sEntry = settings.find(ANDROID_SENSOR_SENSITIVITY);
        rEntry = result.find(ANDROID_SENSOR_SENSITIVITY);
        ASSERT_EQ(sEntry.count, 1);
        ASSERT_EQ(rEntry.count, 1);
        ASSERT_TRUE(abs((float)sEntry.data.i32[0] / rEntry.data.i32[0]- 1.0) <= delta)
                << "set sensitivity " << sEntry.data.i32[0]
                << ", results " << rEntry.data.i32[0];
    }

    void dumpSensorSettings(int frameId, const CameraMetadata &meta) {

        camera_metadata_ro_entry_t exposureEntry = meta.find(ANDROID_SENSOR_EXPOSURE_TIME);
        camera_metadata_ro_entry_t sensitivityEntry = meta.find(ANDROID_SENSOR_SENSITIVITY);
        ASSERT_EQ(exposureEntry.count, 1);
        ASSERT_EQ(sensitivityEntry.count, 1);
        PRINTLN("%s: exposure %ld, sensitivity %d", __func__,
                exposureEntry.data.i64[0], sensitivityEntry.data.i32[0]);
    }

    void buildIspSettings(int settingsNum) {
        if (settingsNum < MAX_FRAME_NUM) {
            settingsNum = MAX_FRAME_NUM;
        }

        camera_metadata_t* requestSettings =
                constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
        CameraMetadata settings;
        settings.acquire(requestSettings);

        uint8_t mode = ANDROID_CONTROL_MODE_OFF;
        settings.update(ANDROID_CONTROL_MODE, &mode, 1);
        mode = ANDROID_CONTROL_AWB_MODE_OFF;
        settings.update(ANDROID_CONTROL_AWB_MODE, &mode, 1);
        mode = ANDROID_COLOR_CORRECTION_MODE_TRANSFORM_MATRIX;
        settings.update(ANDROID_COLOR_CORRECTION_MODE, &mode, 1);
        for (int count = 0; count < settingsNum; count++) {
            // Range of gain: [1, 2]
            float rggb[COLOR_GAIN_LENGTH] = \
                    {1.0f + (float)count / settingsNum, 1.0f, 1.0f, 2.0f - (float)count / settingsNum};
            // Transform: the sum of line should be 1.0
            int32_t colorTransform[TRANSFORM_LENGTH] = {(1 + count) * 500, 0, (1 - count) * 500,
                                         0, 1, 0,
                                         (1 - count) * 500, 0, (1 - count) * 500};
            camera_metadata_rational_t transformMatrix[TRANSFORM_LENGTH];
            for (int idx = 0; idx < TRANSFORM_LENGTH; idx++) {
                transformMatrix[idx].numerator = colorTransform[idx];
                transformMatrix[idx].denominator = 1000;
            }

            settings.update(ANDROID_COLOR_CORRECTION_GAINS, &rggb[0], COLOR_GAIN_LENGTH);
            settings.update(ANDROID_COLOR_CORRECTION_TRANSFORM, &transformMatrix[0], TRANSFORM_LENGTH);
            mSettings[count] = settings;
            dumpIspSettings(count, settings);
        }

        mSettingsNum = settingsNum;
    }

    void checkIspSettings(const CameraMetadata &settings, const CameraMetadata &result) {
        const float delta = 0.02; // %

        camera_metadata_ro_entry_t sEntry = settings.find(ANDROID_COLOR_CORRECTION_GAINS);
        camera_metadata_ro_entry_t rEntry = result.find(ANDROID_COLOR_CORRECTION_GAINS);
        ASSERT_EQ(sEntry.count, COLOR_GAIN_LENGTH);
        ASSERT_EQ(rEntry.count, COLOR_GAIN_LENGTH);
        for (int i = 0; i < sEntry.count; i++) {
            ASSERT_TRUE(abs(rEntry.data.f[i] / sEntry.data.f[i]- 1.0) <= delta)
                    << "gains[ " << i
                    << "] " << sEntry.data.f[i] << " -> " << rEntry.data.f[i];
        }

        sEntry = settings.find(ANDROID_COLOR_CORRECTION_TRANSFORM);
        rEntry = result.find(ANDROID_COLOR_CORRECTION_TRANSFORM);
        ASSERT_EQ(sEntry.count, TRANSFORM_LENGTH);
        ASSERT_EQ(rEntry.count, TRANSFORM_LENGTH);
        for (int i = 0; i < sEntry.count; i++) {
            float sValue = (float)sEntry.data.r[i].numerator / sEntry.data.r[i].denominator;
            float rValue = (float)rEntry.data.r[i].numerator / rEntry.data.r[i].denominator;
            if (abs(sValue) > 0.001) {
                ASSERT_TRUE(abs(rValue / sValue - 1.0) <= delta)
                        << "transform[" << i << "] " << sValue << " -> " << rValue;
            } else {
                // In case sValue is 0 (or close to)
                ASSERT_TRUE(abs(rValue - sValue) <= delta)
                        << "transform[" << i << "] " << sValue << " -> " << rValue;
            }
        }
    }

    void dumpIspSettings(int frameId, const CameraMetadata &meta) {
        camera_metadata_ro_entry_t gains = meta.find(ANDROID_COLOR_CORRECTION_GAINS);
        camera_metadata_ro_entry_t transform = meta.find(ANDROID_COLOR_CORRECTION_TRANSFORM);
        ASSERT_EQ(gains.count, COLOR_GAIN_LENGTH);
        ASSERT_EQ(transform.count, TRANSFORM_LENGTH);
        PRINTLN("%s: frame %d, gains [%6.4f, %6.4f, %6.4f], ccm [(%6.4f %6.4f %6.4f)  (%6.4f %6.4f %6.4f)  (%6.4f %6.4f %6.4f)]",
                __func__, frameId,
                gains.data.f[0], gains.data.f[1], gains.data.f[2],
                (float)transform.data.r[0].numerator / transform.data.r[0].denominator,
                (float)transform.data.r[1].numerator / transform.data.r[1].denominator,
                (float)transform.data.r[2].numerator / transform.data.r[2].denominator,
                (float)transform.data.r[3].numerator / transform.data.r[3].denominator,
                (float)transform.data.r[4].numerator / transform.data.r[4].denominator,
                (float)transform.data.r[5].numerator / transform.data.r[5].denominator,
                (float)transform.data.r[6].numerator / transform.data.r[6].denominator,
                (float)transform.data.r[7].numerator / transform.data.r[7].denominator,
                (float)transform.data.r[8].numerator / transform.data.r[8].denominator);
    }

public:
    Paramz::TestParam *mParam;
    const camera_metadata_t *mStatics;

    int mSettingsNum;
    CameraMetadata mSettings[MAX_FRAME_NUM];
    CameraMetadata mResults[MAX_FRAME_NUM];

    camera3_stream_t mStreams[MAX_STREAM_NUM];

    int mRequestCount;
    int mResultCount;
    camera3_stream_buffer mStreamBuffer;
};

TEST_P(Perframe_Test, CheckPerframeResult)
{
    int requestCount = MAX_FRAME_NUM;
    configureStreams();
    sendRequests(8);
    checkResults(8);
}

TEST_P(Perframe_Test, TestPerFrameSensorSettings)
{
    bool perFrame = hasMetadataValue<uint8_t>(ANDROID_SYNC_MAX_LATENCY,
                                              ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL,
                                              mStatics);
    bool manualAe = hasMetadataValue<uint8_t>(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                                              ANDROID_CONTROL_AE_MODE_OFF,
                                              mStatics);
    if (!(perFrame && manualAe)) {
        return;
    }

    configureStreams();
    int settingsCount = MAX_FRAME_NUM;
    buildSensorSettings(settingsCount);
    sendRequests(settingsCount, true);
    checkResults(settingsCount, TEST_SENSOR_SETTINGS);
}

TEST_P(Perframe_Test, TestPerFrameIspSettings)
{
    bool perFrame = hasMetadataValue<uint8_t>(ANDROID_SYNC_MAX_LATENCY,
                                              ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL,
                                              mStatics);
    bool manualAwb = hasMetadataValue<uint8_t>(ANDROID_CONTROL_AWB_AVAILABLE_MODES,
                                              ANDROID_CONTROL_AWB_MODE_OFF,
                                              mStatics);
    if (!(perFrame && manualAwb)) {
        return;
    }

    configureStreams();
    int settingsCount = MAX_FRAME_NUM;
    buildIspSettings(settingsCount);
    sendRequests(settingsCount, true);
    checkResults(settingsCount, TEST_ISP_SETTINGS);
}

TEST_P(Perframe_Test, TestPerFrameSensorSettingsAndAutoCombination)
{
    bool perFrame = hasMetadataValue<uint8_t>(ANDROID_SYNC_MAX_LATENCY,
                                              ANDROID_SYNC_MAX_LATENCY_PER_FRAME_CONTROL,
                                              mStatics);
    bool manualAe = hasMetadataValue<uint8_t>(ANDROID_CONTROL_AE_AVAILABLE_MODES,
                                              ANDROID_CONTROL_AE_MODE_OFF,
                                              mStatics);
    if (!(perFrame && manualAe)) {
        return;
    }

    configureStreams();

    // Loop1: per-frame
    int settingsCount = MAX_FRAME_NUM;
    buildSensorSettings(settingsCount);
    sendRequests(settingsCount, true);
    int totalFrameCount = settingsCount;
    checkResults(totalFrameCount, TEST_SENSOR_SETTINGS);

    // Loop2: auto
    sendRequests(8);
    totalFrameCount += 8;
    checkResults(totalFrameCount);

    // Loop3: per-frame
    sendRequests(settingsCount, true);
    totalFrameCount += settingsCount;
    checkResults(totalFrameCount, TEST_SENSOR_SETTINGS);
}

INSTANTIATE_TEST_CASE_P(perframe,
                        Perframe_Test,
                        ::testing::ValuesIn(Parmz::getCameraValues()));

