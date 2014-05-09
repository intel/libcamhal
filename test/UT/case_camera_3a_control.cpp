/*
 * Copyright (C) 2018 Intel Corporation.
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

#define LOG_TAG "CASE_3A_CONTROL"

#include <math.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "ICamera.h"
#include "Parameters.h"
#include "case_common.h"

#define STREAM_NUM     1
#define MAX_FRAME_NUM  10

#define MAX_RESULT_NUM 10

enum TestType {
    TEST_AE,
    TEST_AWB,
    TEST_AF
};

struct TestContent {
    // settings
    int step;
    TestType type;
    uint8_t mode;
    uint8_t trigger;

    // possible results
    int possibleStateCount;
    uint8_t state[MAX_RESULT_NUM];
};

class Cam3AControlTest: public testing::Test {
protected:
    Cam3AControlTest() {}

    virtual void SetUp() {
        mCameraId = getCurrentCameraId();
        camera_info_t info;
        get_camera_info(mCameraId, info);
        if (info.capability) {
            info.capability->getSupportedAfMode(mAfModes);
        }
    }

    int getStreamConfiguration() {
        if (prepareStreams(mCameraId, mStream, STREAM_NUM) != 0) {
            camera_info_t info;
            int ret = get_camera_info(mCameraId, info);
            supported_stream_config_array_t configs;
            info.capability->getSupportedStreamConfig(configs);
            mStream[0] = getStreamByConfig(configs[0]);
            LOGD("@%s, preset stream not supported, use stream: format:%s (%dx%d) field=%d",
                    __func__,
                    CameraUtils::format2string(mStream[0].format), mStream[0].width,
                        mStream[0].height, mStream[0].field);
        }
        return 0;
    }

    int allocateBuffers(int bufNum) {
        int ret = 0;
        const int page_size = getpagesize();
        mBufferNum = bufNum;
        for (int i = 0; i < mBufferNum; i++) {
            camera_buffer_t &buffer = mBuffers[i];
            buffer.s = mStream[0];
            buffer.s.size = CameraUtils::getFrameSize(mStream[0].format, mStream[0].width, mStream[0].height);
            buffer.flags = 0;

            ret = posix_memalign(&buffer.addr, page_size, buffer.s.size);
            EXPECT_TRUE((buffer.addr != nullptr) && (ret == 0));
        }
        return ret;
    }

    void freeBuffers() {
        for (int i = 0; i < mBufferNum; i++) {
            camera_buffer_t &buffer = mBuffers[i];
            free(buffer.addr);
            buffer.addr = nullptr;
        }
        CLEAR(mBuffers);
    }

    void constructSettings(const TestContent& content, Parameters &settings) {
        if (content.type == TEST_AF) {
            LOGD("%s: step %d, af mode %d, trigger %d", __func__,
                    content.step, content.mode, content.trigger);
            settings.setAfMode((camera_af_mode_t)content.mode);
            settings.setAfTrigger((camera_af_trigger_t)content.trigger);
        }
    }

    bool checkResult(const Parameters &result, const TestContent& content) {
        if (!content.possibleStateCount) {
            LOG2("%s: no checking", __func__);
            return true;
        }

        camera_af_state_t state = AF_STATE_FAIL;
        if (content.type == TEST_AF) {
            result.getAfState(state);
            LOG2("%s: get af state %d", __func__, state);
        }
        for (int i = 0; i < content.possibleStateCount; i++) {
            if (state == content.state[i]) {
                return true;
            }
        }
        return false;
    }

    void test_3a_control(int totalTestSteps, TestContent* contents, int maxFrameRun = 100);

    int mCameraId;
    std::vector<camera_af_mode_t> mAfModes;

    stream_t mStream[STREAM_NUM];
    int mBufferNum = MAX_FRAME_NUM;
    camera_buffer_t mBuffers[MAX_FRAME_NUM];

    vector<Parameters*> mSettings;
    vector<Parameters*> mResults;
};

void Cam3AControlTest::test_3a_control(int totalTestSteps, TestContent* contents, int maxFrameRun)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_device_open(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = getStreamConfiguration();
    EXPECT_EQ(ret, 0);

    stream_config_t streamList;
    streamList.num_streams = STREAM_NUM;
    streamList.streams = mStream;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_NORMAL;
    ret = camera_device_config_streams(mCameraId, &streamList);
    EXPECT_EQ(ret, 0);

    ret = allocateBuffers(4);
    EXPECT_EQ(ret, 0);

    int step = 0;
    int requestCount = -1;
    int idRequestInTest = maxFrameRun + 1;
    int resultCount = -1;

    camera_buffer_t* buffer = nullptr;
    for (int i = 0; i < mBufferNum; i++) {
        buffer = &mBuffers[i];
        ret = camera_stream_qbuf(mCameraId, &buffer, 1);
        EXPECT_EQ(ret, 0);
        requestCount++;
    }
    ret = camera_device_start(mCameraId);
    EXPECT_EQ(ret, 0);

    bool currentStepDone = true;
    Parameters settings;
    Parameters results;
    while (maxFrameRun-- && (step < totalTestSteps)) {
        ret = camera_stream_dqbuf(mCameraId, mStream[0].id, &buffer, &results);
        EXPECT_EQ(ret, 0);
        resultCount++;

        if (resultCount >= idRequestInTest) {
            currentStepDone = checkResult(results, contents[step]);
            if (currentStepDone) {
                LOGD("[TEST]  step %d in request %d, done in result %d", step, idRequestInTest, resultCount);
            }
        }

        if (currentStepDone) {
            step++;
            constructSettings(contents[step], settings);

            ret = camera_stream_qbuf(mCameraId, &buffer, 1, &settings);
            EXPECT_EQ(ret, 0);
            requestCount++;

            currentStepDone = false;
            idRequestInTest = requestCount;
            LOGD("[TEST]  step %d in request %d", step, idRequestInTest);
        } else {
            ret = camera_stream_qbuf(mCameraId, &buffer, 1, &settings);
            EXPECT_EQ(ret, 0);
            requestCount++;
        }
    }

    EXPECT_EQ(step, totalTestSteps);

    ret = camera_device_stop(mCameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(mCameraId);

    freeBuffers();

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(Cam3AControlTest, camera_device_auto_af_trigger)
{
    bool support = false;
    for (auto mode : mAfModes) {
        if (mode == AF_MODE_AUTO) {
            support  = true;
            break;
        }
    }
    if (!support) {
        LOGD("%s: Skip test due to no auto af mode", __func__);
        return;
    }

    TestContent autoAfTest[] = {
        // Initial state
        {0, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_IDLE,   1, {AF_STATE_IDLE}},
        // Trigger
        {1, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_START,  0, {0}},
        {2, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_IDLE,   4, {AF_STATE_LOCAL_SEARCH, AF_STATE_EXTENDED_SEARCH, AF_STATE_SUCCESS, AF_STATE_FAIL}},
         // Searching done
        {3, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_IDLE,   2, {AF_STATE_SUCCESS, AF_STATE_FAIL}},
        // Trigger again
        {4, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_START,  0, {0}},
        {5, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_IDLE,   4, {AF_STATE_LOCAL_SEARCH, AF_STATE_EXTENDED_SEARCH, AF_STATE_SUCCESS, AF_STATE_FAIL}},
        // Cancel
        {6, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_CANCEL, 0, {0}},
        {7, TEST_AF, AF_MODE_AUTO, AF_TRIGGER_IDLE,   1, {AF_STATE_IDLE}}
    };

    test_3a_control(sizeof(autoAfTest)/sizeof(autoAfTest[0]), autoAfTest);
}

TEST_F(Cam3AControlTest, camera_device_continuous_af_trigger)
{
    bool support = false;
    for (auto mode : mAfModes) {
        if (mode == AF_MODE_CONTINUOUS_VIDEO) {
            support  = true;
            break;
        }
    }
    if (!support) {
        LOGD("%s: Skip test due to no auto af mode", __func__);
        return;
    }

    TestContent continuousAfTest[] = {
        // Initial state
        {0, TEST_AF, AF_MODE_CONTINUOUS_VIDEO, AF_TRIGGER_IDLE,   0, {0}},
        // Internal scan
        {1, TEST_AF, AF_MODE_CONTINUOUS_VIDEO, AF_TRIGGER_IDLE,   2, {AF_STATE_LOCAL_SEARCH, AF_STATE_EXTENDED_SEARCH}},
         // Trigger
        {2, TEST_AF, AF_MODE_CONTINUOUS_VIDEO, AF_TRIGGER_START,  0, {0}},
        {3, TEST_AF, AF_MODE_CONTINUOUS_VIDEO, AF_TRIGGER_IDLE,   0, {AF_STATE_SUCCESS, AF_STATE_FAIL}},
        // Cancel
        {4, TEST_AF, AF_MODE_CONTINUOUS_VIDEO, AF_TRIGGER_CANCEL, 0, {0}},
        {5, TEST_AF, AF_MODE_CONTINUOUS_VIDEO, AF_TRIGGER_IDLE,   2, {AF_STATE_LOCAL_SEARCH, AF_STATE_EXTENDED_SEARCH}}
    };

    test_3a_control(sizeof(continuousAfTest)/sizeof(continuousAfTest[0]), continuousAfTest);
}

