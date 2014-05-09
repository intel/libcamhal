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
#define LOG_TAG "3atest"
#include "raw_hal_test.h"

namespace TSF = TestStreamFactory;
namespace Parmz = Parameterization;

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

class Control3A_Test : public ::Basic_Test, public ::testing::WithParamInterface<Paramz::TestParam> {
public:
    Control3A_Test() :
    mParam(nullptr),
    mStatics(nullptr),
    mCurrentContent(nullptr),
    mResultCount(-1),
    mStepDone(false) {}

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

    void TearDown() {
        mParam = NULL;
        Basic_Test::TearDown();
    }

    void configureStreams(int bufferNum = 4)
    {
        camera3_stream_configuration_t streamConfig;
        camera3_stream_t *streamPtrs[1];
        camera3_capture_request_t request;

        // Configure streams
        status_t status = createSingleStreamConfig(streamConfig, mStream, streamPtrs, 1920, 1080);
        ASSERT_EQ(status, 0) << "HAL stream config failed status: " \
                             << std::hex <<  status;

        // Allocate memory
        status = allocateBuffers(bufferNum, &mStream);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        camera_metadata_t* requestSettings =
                constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);
        mSettings.acquire(requestSettings);
    }

    void sendRequest(int reqId, const CameraMetadata* settings = nullptr)
    {
        camera3_capture_request_t request;
        std::unique_lock<std::mutex> lock(mTestMutex);

        if (mBuffers.size() == 0) {
            mTestCondition.waitRelative(lock, 1.0f * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
            if (mBuffers.size() == 0) {
                // TODO: asserts not good here in sub-functions
                ASSERT_TRUE(false) << "timed out waiting for buffers";
            }
        }

        request.frame_number = reqId;
        request.input_buffer = NULL;
        mStreamBuffer = mBuffers[0];
        mBuffers.erase(mBuffers.begin());
        request.output_buffers = &mStreamBuffer;
        request.num_output_buffers = 1;
        if (settings)
            request.settings = settings->getAndLock();
        else
            request.settings = nullptr;

        status_t status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
        ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                             << std::hex <<  status;

        if (request.settings)
            settings->unlock(request.settings);
    }

    void constructSettings(const TestContent* content, CameraMetadata &settings) {
        ASSERT_NE(content, nullptr) << "null test content";

        if (content->type == TEST_AF) {
            PRINTLN("[Test] %s: step %d, af mode %d, trigger %d", __func__,
                    content->step, content->mode, content->trigger);
            settings.update(ANDROID_CONTROL_AF_MODE, &content->mode, 1);
            settings.update(ANDROID_CONTROL_AF_TRIGGER, &content->trigger, 1);
        }
    }

    void checkResult(const camera_metadata_t *result, const TestContent* content) {
        if (!content || mStepDone)
            return;

        if (!content->possibleStateCount) {
            mStepDone = true;
            PRINTLN("%s: no check for step %d", __func__, content->step);
            return;
        }

        camera_metadata_ro_entry_t entry;
        uint8_t state = 0;
        int ret = 0;
        if (content->type == TEST_AF) {
            ret = find_camera_metadata_ro_entry(result, ANDROID_CONTROL_AF_STATE, &entry);
            if (ret != 0)
                return;

            state = entry.data.u8[0];
            PRINTLN("%s: get af state %d", __func__, state);
        }
        for (int i = 0; i < content->possibleStateCount; i++) {
            if (state == content->state[i]) {
                mStepDone = true;
                PRINTLN("zj [TEST] %s: step %d done in result %d, state %d",
                        __func__, content->step, mResultCount, state);
            }
        }
    }

    void processCaptureResult(const camera3_capture_result_t *result)
    {
        if (result && result->result) {
            mResultCount = result->frame_number;
            checkResult(result->result, mCurrentContent);
        }

        Basic_Test::processCaptureResult(result);
    }

    void test_3a_control(int totalTestSteps, TestContent* contents, int maxFrameRun = 100);

public:
    Paramz::TestParam *mParam;
    const camera_metadata_t *mStatics;

    camera3_stream_t mStream;
    camera3_stream_buffer mStreamBuffer;
    CameraMetadata mSettings;

    // For capture result
    bool mStepDone;
    int mResultCount;
    TestContent* mCurrentContent;
};

void Control3A_Test::test_3a_control(int totalTestSteps, TestContent* contents, int maxFrameRun)
{
    configureStreams();

    int nextStep = 0;
    int idRequestInTest = maxFrameRun + 1;
    int requestCount = -1;
    mResultCount = -1;
    mStepDone = true;
    mCurrentContent = nullptr;

    do {
        requestCount++;
        if (requestCount == 0) {
            sendRequest(requestCount, &mSettings);
        } else {
            sendRequest(requestCount);
        }
    } while (requestCount < 4 && mBuffers.size() > 0);

    while (maxFrameRun-- && (nextStep < totalTestSteps)) {
        if (mStepDone) {
            mCurrentContent = &contents[nextStep];
            constructSettings(mCurrentContent, mSettings);
            requestCount++;
            sendRequest(requestCount, &mSettings);
            PRINTLN("[TEST] step %d in request %d", nextStep, requestCount);

            mStepDone = false;
            nextStep++;
        } else {
            requestCount++;
            sendRequest(requestCount);
        }
    }
    mCurrentContent = nullptr;
    EXPECT_EQ(nextStep, totalTestSteps);
}

TEST_P(Control3A_Test, TestAutoAfTrigger)
{
    bool autoAf = hasMetadataValue<uint8_t>(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                                            ANDROID_CONTROL_AF_MODE_AUTO,
                                            mStatics);
    if (!autoAf) {
        PRINTLN("%s: Skip test due to no auto af mode", __func__);
        return;
    }

    TestContent autoAfTest[] = {
        // Initial state
        {0, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    1, {ANDROID_CONTROL_AF_STATE_INACTIVE}},
        // Trigger
        {1, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_START,
                    0, {0}},
        {2, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    3, {ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN, ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED, ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED}},
         // Searching done
        {3, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    2, {ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED, ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED}},
        // Trigger again
        {4, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_START,
                    0, {0}},
        {5, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    3, {ANDROID_CONTROL_AF_STATE_ACTIVE_SCAN, ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED, ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED}},
        // Cancel
        {6, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_CANCEL,
                    0, {0}},
        {7, TEST_AF, ANDROID_CONTROL_AF_MODE_AUTO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    1, {ANDROID_CONTROL_AF_STATE_INACTIVE}}
    };

    test_3a_control(sizeof(autoAfTest)/sizeof(autoAfTest[0]), autoAfTest);
}

TEST_P(Control3A_Test, TestContinuousAfTrigger)
{
    bool continuousAf = hasMetadataValue<uint8_t>(ANDROID_CONTROL_AF_AVAILABLE_MODES,
                                                  ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO,
                                                  mStatics);
    if (!continuousAf) {
        PRINTLN("%s: Skip test due to no continuous af mode", __func__);
        return;
    }

    TestContent continuousAfTest[] = {
        // Initial state
        {0, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    0, {0}},
        // Internal scan
        {1, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    3, {ANDROID_CONTROL_AF_STATE_PASSIVE_SCAN, ANDROID_CONTROL_AF_STATE_PASSIVE_FOCUSED, ANDROID_CONTROL_AF_STATE_PASSIVE_UNFOCUSED}},
        {2, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    2, {ANDROID_CONTROL_AF_STATE_PASSIVE_FOCUSED, ANDROID_CONTROL_AF_STATE_PASSIVE_UNFOCUSED}},
         // Trigger
        {3, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_START,
                    0, {0}},
        {4, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    2, {ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED, ANDROID_CONTROL_AF_STATE_NOT_FOCUSED_LOCKED}},
        // Cancel
        {5, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_CANCEL,
                    0, {0}},
        {6, TEST_AF, ANDROID_CONTROL_AF_MODE_CONTINUOUS_VIDEO, ANDROID_CONTROL_AF_TRIGGER_IDLE,
                    2, {ANDROID_CONTROL_AF_STATE_INACTIVE, ANDROID_CONTROL_AF_STATE_PASSIVE_SCAN}}
    };

    test_3a_control(sizeof(continuousAfTest)/sizeof(continuousAfTest[0]), continuousAfTest);
}

INSTANTIATE_TEST_CASE_P(control3A,
                        Control3A_Test,
                        ::testing::ValuesIn(Parmz::getCameraValues()));

