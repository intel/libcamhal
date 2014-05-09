/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#define LOG_TAG "CASE_DUAL"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "linux/videodev2.h"
#include "iutils/Utils.h"
#include "iutils/CameraLog.h"
#include "ICamera.h"
#include "MockSysCall.h"
#include "case_common.h"
#include "PlatformData.h"

using namespace icamera;

class camHalDualTest: public testing::Test {
public:
    camHalDualTest() {}

    void camhal_qbuf_dqbuf_dual(int main_width, int main_height, int main_fmt, int main_field,
                                int second_width, int second_height, int second_fmt, int second_field,
                                int alloc_buf_count, ParamList* params,
                                int main_dq_buf_count, int second_dq_buf_count,
                                unsigned main_cam_sleep_time, unsigned second_cam_sleep_time,
                                unsigned second_cam_run_times)
    {
        /* init camera device */
        int ret = camera_hal_init();
        EXPECT_EQ(ret, 0);

        int mainCamId = 0;
        int secondCamId = 0;

        int camNum = get_number_of_cameras();
        LOGD("%s,camNum:%d", __func__, camNum);
        for (int i = 0; i < camNum; i++) {
            camera_info_t info;
            get_camera_info(i, info);
            LOGD("%s, the cameraId:%d sensor's name:%s", __func__, i, info.name);
            if (!mMainCamName.compare(info.name)) {
                LOGD("%s, the main camera, the i:%d sensor's name:%s", __func__, i, info.name);
                mainCamId = i;
                continue;
            }

            if (!mSecondCamName.compare(info.name)) {
                LOGD("%s, the second camera, the i:%d sensor's name:%s", __func__, i, info.name);
                secondCamId = i;
                continue;
            }
        }

        LOGD("%s,mainCameraId:%d, secondCameraId:%d", __func__, mainCamId, secondCamId);
        EXPECT_TRUE(mainCamId != secondCamId);

        pthread_t mainCamThread;
        struct camThreadInfo mainCamInfo = {mainCamId, main_width, main_height, main_fmt, alloc_buf_count,
                                        main_dq_buf_count, main_field, params, main_cam_sleep_time};
        pthread_create(&mainCamThread, NULL, cam_thread, &mainCamInfo);

        while (second_cam_run_times--) {
            pthread_t secondCamThread;
            struct camThreadInfo secondCamInfo = {secondCamId, second_width, second_height, second_fmt, alloc_buf_count,
                                            second_dq_buf_count, second_field, params, second_cam_sleep_time};
            pthread_create(&secondCamThread, NULL, cam_thread, &secondCamInfo);
            pthread_join(secondCamThread, NULL);
        }

        pthread_join(mainCamThread, NULL);

        /* deinit camera device */
        ret = camera_hal_deinit();
        EXPECT_EQ(ret, 0);

        SUCCEED();
    }

    bool needSkipTest() {
        getEnvVals();

        bool hasMainCam = false;
        for (auto & name : mSupportedMainCamName) {
            if (!mMainCamName.compare(name)) {
                hasMainCam = true;
            }
        }

        bool hasSecondCam = false;
        for (auto & name : mSupportedSecondCamName) {
            if (!mSecondCamName.compare(name)) {
                hasSecondCam = true;
            }
        }

        if (!hasMainCam || !hasSecondCam) {
            LOGD("%s, mMainCamName:%s, mSecondCamName:%s, hasMainCam:%d, hasSecondCam:%d",
                    __func__, mMainCamName.c_str(), mSecondCamName.c_str(), hasMainCam, hasSecondCam);
            return true;
        }

        return false;
    }

    struct TestDualInfo {
        int main_dq_buf_cnt;
        int second_dq_buf_cnt;
        unsigned main_sleep_time; // the time is second
        unsigned second_sleep_time;
        unsigned second_run_times;
    };

    void main_test(int main_width, int main_height, int main_fmt, int main_field,
                    int second_width, int second_height, int second_fmt, int second_field,
                    vector<struct TestDualInfo>& info) {
        for (auto & val : info) {
            LOGD("@%s, main_dq_buf_cnt:%d, second_dq_buf_cnt:%d, main_sleep_time:%d, second_sleep_time:%d",
                    __func__, val.main_dq_buf_cnt, val.second_dq_buf_cnt, val.main_sleep_time, val.second_sleep_time);
            camhal_qbuf_dqbuf_dual(main_width, main_height, main_fmt, main_field, second_width, second_height, second_fmt, second_field, 8, nullptr,
                                    val.main_dq_buf_cnt, val.second_dq_buf_cnt,
                                    val.main_sleep_time, val.second_sleep_time, val.second_run_times);
        }
    }

private:
    void getEnvVals() {
        char* cameraMipiCapture = getenv("cameraMipiCapture");
        if (cameraMipiCapture && !strncmp(cameraMipiCapture, "true", sizeof("true"))) {
            LOGD("%s, the cameraMipiCapture is true", __func__);
            mCameraMipiCapture = true;
        } else {
            LOGD("%s, the cameraMipiCapture is false", __func__);
            mCameraMipiCapture = false;
        }

        char* cameraName = getenv("cameraInput");
        if (cameraName) {
            LOGD("%s, the cameraInput is %s", __func__, cameraName);
            mMainCamName = cameraName;
        }

        char* camera2Name = getenv("cameraInput2");
        if (camera2Name) {
            LOGD("%s, the cameraInput2 is %s", __func__, camera2Name);
            mSecondCamName = camera2Name;
        }
    }

    struct camThreadInfo {
        int cameraId;
        int width;
        int height;
        int fmt;
        int alloc_buf_count;
        int dq_buf_count;
        int field;
        ParamList* params;
        unsigned sleep_time; // the unit is second
    };

    static void* cam_thread(void* arg) {
        struct camThreadInfo* info = (camThreadInfo*)arg;
        if (info->sleep_time) {
            sleep(info->sleep_time);
        }

        LOGD("@%s, cameraId:%d, start", __func__, info->cameraId);
        camhal_qbuf_dqbuf(info->cameraId, info->width, info->height, info->fmt,
                            info->alloc_buf_count, info->dq_buf_count, info->field, info->params);
        LOGD("@%s, cameraId:%d, end", __func__, info->cameraId);

        return NULL;
    }

    string mMainCamName;
    string mSecondCamName;
    const vector<string> mSupportedMainCamName = {
        "mondello",
    };
    const vector<string> mSupportedSecondCamName = {
        "mondello-2",
    };
    bool mCameraMipiCapture;
};

// for UYVY
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_1080p_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_1080p_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_1080p_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_1080p_second_run_5_times)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {200, 20, 0, 0, 5},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_720p)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_720p_second_run_5_times)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {200, 20, 0, 0, 5},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_vga)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_720x576)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_interlaced_1080i)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_interlaced_576i)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_UYVY_interlaced_480i)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

// for YUYV
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_1080p_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_1080p_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_1080p_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_720p)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_vga)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_720x576)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_interlaced_1080i)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_interlaced_576i)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_YUYV_interlaced_480i)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

// for RGB888 which is aligned by 24 bit
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_BG24_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_BG24_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_BG24_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_720p_BG24)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_vga_BG24)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_720x576_BG24)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_interlaced_1080i_BG24)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_interlaced_576i_BG24)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_interlaced_480i_BG24)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_BGR24;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

// for RGB888 which is aligned by 32 bit
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_XBGR32_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_XBGR32_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_XBGR32_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_1080p_XBGR32_second_run_5_times)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {200, 20, 0, 0, 5},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_720p_XBGR32)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_720p_XBGR32_second_run_5_times)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {200, 20, 0, 0, 5},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_vga_XBGR32)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_720x576_XBGR32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_interlaced_1080i_XBGR32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_interlaced_576i_XBGR32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb888_interlaced_480i_XBGR32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

// for RGB565 which is aligned by 16 bit
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_1080p_RGB565_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_1080p_RGB565_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_1080p_RGB565_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_720p_RGB565)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_vga_RGB565)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_720x576_RGB565)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_interlaced_1080i_RGB565)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_interlaced_576i_RGB565)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_interlaced_480i_RGB565)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_RGB565;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

// for RGB565 which is aligned by 32 bit
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_1080p_XRGB32_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_1080p_XRGB32_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_1080p_XRGB32_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_720p_XRGB32)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_vga_XRGB32)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_720x576_XRGB32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_interlaced_1080i_XRGB32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_interlaced_576i_XRGB32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_rgb565_interlaced_480i_XRGB32)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

// for NV16
TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_NV16_1080p_normal)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_NV16_1080p_first_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 1, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_NV16_1080p_second_delay)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {50, 50, 0, 1, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_NV16_720p)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_NV16_vga)
{
    if (needSkipTest()) {
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_NV16_720x576)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_nv16_interlaced_1080i_NV16)
{
    if (needSkipTest()) {
        return;
    }

    int width = 1920;
    int height = 1080;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_nv16_interlaced_576i_NV16)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}

TEST_F(camHalDualTest, dual_mondello_qbuf_dqbuf_nv16_interlaced_480i_NV16)
{
    if (needSkipTest()) {
        return;
    }

    int width = 720;
    int height = 480;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ALTERNATE;
    vector<struct TestDualInfo> testInfo = {
        {20, 20, 0, 0, 1},
    };

    main_test(width, height, fmt, field, width, height, fmt, field, testInfo);
}
