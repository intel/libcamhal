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

/*
 * This file has some HAL unit test cases as workload for PnP test.
 */

#include <gtest/gtest.h>

#include "test_utils.h"
#include "pnp_aal_test.h"

TEST_P(PnPHal_Test, SingleStreamCapture) {
    uint32_t status = 0;
    camera3_stream_configuration_t streamConfig;
    camera3_stream_t streams[1];
    camera3_stream_t *streamPtrs[1];
    camera3_capture_request_t request;
    camera3_stream_buffer streamBuffer;

    PRINTLN("%dx%d %d fps test running for %d frames", mPnPParam.width,
		mPnPParam.height, mPnPParam.framerate, mPnPParam.frameCnt);

    status = createSingleStreamConfig(streamConfig, streams[0], streamPtrs,
		mPnPParam.width, mPnPParam.height);
    ASSERT_EQ(status, 0) << "HAL stream config failed status: " \
                     << std::hex <<  status;

    camera_metadata_t *requestSettings =
            constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

    // allocate memory
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
    for (int i = 0; i < mPnPParam.frameCnt; i++) {
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
    float fps = mPnPParam.frameCnt * 1000000.0f / microseconds;
    PRINTLN("%dx%d %dfps test ran with %f fps", mPnPParam.width, mPnPParam.height,
		mPnPParam.framerate, fps);

    free_camera_metadata(requestSettings);
}

INSTANTIATE_TEST_CASE_P(PnP,
                        PnPHal_Test,
                        ::testing::ValuesIn(PnPHal_Test::getPnPHAL_TestParam()));
