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

#define LOG_TAG "CASE_VIRTUAL_CHANNEL"

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
#define MAX_TEST_CAM_NUM 4

class camHalVirtualChannelTest: public testing::Test {
public:
    camHalVirtualChannelTest() {}

    struct TestInfo {
        int dq_buf_cnt[MAX_TEST_CAM_NUM];
        unsigned sleep_time[MAX_TEST_CAM_NUM]; // the time is second
    };

    void camhal_qbuf_dqbuf_vc(int width, int height, int fmt, int alloc_buf_count,
                                int field, ParamList* params, TestInfo& info)
    {
        const int inputNum = mEnvInfo.size();
        int validVCCamNum = 0;
        /* init camera device */
        int ret = camera_hal_init();
        EXPECT_EQ(ret, 0);

        int cameraId[inputNum];

        int camNum = get_number_of_cameras();
        LOGD("%s,camNum:%d", __func__, camNum);
        for (int i = 0; i < camNum; i++) {
            camera_info_t camInfo;
            get_camera_info(i, camInfo);
            LOGD("%s, the cameraId:%d sensor's name:%s", __func__, i, camInfo.name);

            if (!camInfo.vc.total_num)
                continue;

            for (auto & envInfo : mEnvInfo) {
                if (!envInfo.name.compare(camInfo.name)) {
                    VcInfo vcInfo = {i, camInfo.name, camInfo.vc.sequence, camInfo.vc.total_num};
                    LOGD("%s, VcInfo: id:%d, name:%s, sequence:%d, total_num:%d", __func__,
                        vcInfo.id, vcInfo.name.c_str(), vcInfo.sequence, vcInfo.total_num);
                    mVcInfo.push_back(vcInfo);
                    break;
                }
            }
        }

        int testNum = std::min((int)mVcInfo.size(), inputNum);
        LOGD("@%s, virtual channel camera num:%ld, input camera num:%d, test num:%d", __func__, mVcInfo.size(), inputNum, testNum);

        vector <pthread_t> camThreads;
        struct CamThreadInfo threadInfo[testNum];

        for (int i = 0; i < testNum; i++) {
            pthread_t camThread;
            threadInfo[i] = {testNum, mVcInfo[i].id, width, height, fmt, alloc_buf_count,
                                info.dq_buf_cnt[i], field, params, info.sleep_time[i]};

            pthread_create(&camThread, NULL, cam_thread, &threadInfo[i]);
            camThreads.push_back(camThread);
        }

        for (auto & t : camThreads) {
            pthread_join(t, NULL);
        }

        /* deinit camera device */
        ret = camera_hal_deinit();
        EXPECT_EQ(ret, 0);

        SUCCEED();
    }

    bool needSkipTest() {
        getEnvVals();

        for (auto & info : mEnvInfo) {
            bool match = false;
            for (const string & one : mSupportedCamName) {
                if (!info.name.compare(one)) {
                    match = true;
                    break;
                }
            }
            if (match == false) {
                LOGD("@%s, the input camera %s is not found", __func__, info.name.c_str());
                return true;
            }
        }

        LOGD("%s, the input camera total num is :%ld", __func__, mEnvInfo.size());
        for (auto & info : mEnvInfo) {
            LOGD("@%s, input camera: %s", __func__, info.name.c_str());
        }

        return false;
    }

    void main_test(int width, int height, int fmt, int field, vector<struct TestInfo>& infos) {
        for (auto & info : infos) {
            for (int i = 0; i < mEnvInfo.size(); i++) {
                LOGD("@%s, i:%d, dq_buf_cnt:%d, sleep_time:%d", __func__, i, info.dq_buf_cnt[i], info.sleep_time[i]);
            }

            camhal_qbuf_dqbuf_vc(width, height, fmt, 8, field, nullptr, info);
        }
    }

private:
    void getEnvVals() {
        char* cameraMipiCapture = getenv("cameraMipiCapture");
        if (cameraMipiCapture && !strncmp(cameraMipiCapture, "true", sizeof("true"))) {
            LOGD("%s, the cameraMipiCapture is true", __func__);
            mCamMipiCapture = true;
        } else {
            LOGD("%s, the cameraMipiCapture is false", __func__);
            mCamMipiCapture = false;
        }

        for (int i = 1; i <= MAX_TEST_CAM_NUM; i++) {
            string env = "cameraInput";
            if (i > 1) {
                env += std::to_string(i);
            }
            char* name = getenv(env.c_str());
            if (name) {
                EnvInfo info = {env, name};
                mEnvInfo.push_back(info);
            }

        }
    }

    struct CamThreadInfo {
        int totalVirtualChannelCamNum;

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
        struct CamThreadInfo* info = (CamThreadInfo*)arg;
        if (info->sleep_time) {
            sleep(info->sleep_time);
        }

        LOGD("@%s, cameraId:%d, info->totalVirtualChannelCamNum:%d", __func__,
            info->cameraId, info->totalVirtualChannelCamNum);
        LOGD("@%s, cameraId:%d, info->width:%d, height:%d, format:%s", __func__,
            info->cameraId, info->width, info->height, CameraUtils::format2string(info->fmt));
        LOGD("@%s, cameraId:%d, info->alloc_buf_count:%d", __func__, info->cameraId, info->alloc_buf_count);
        LOGD("@%s, cameraId:%d, info->dq_buf_count:%d", __func__, info->cameraId, info->dq_buf_count);
        LOGD("@%s, cameraId:%d, info->sleep_time:%d", __func__, info->cameraId, info->sleep_time);

        camhal_qbuf_dqbuf(info->cameraId, info->width, info->height, info->fmt,
                            info->alloc_buf_count, info->dq_buf_count, info->field,
                            info->params, info->totalVirtualChannelCamNum);
        LOGD("@%s, cameraId:%d, cam_thread end", __func__, info->cameraId);

        return NULL;
    }

    string mCamNames[MAX_TEST_CAM_NUM];
    const vector<string> mSupportedCamName = {
        "aggregator",
        "aggregator-2",
        "aggregator-3",
        "aggregator-4",
        "ov10635-vc",
        "ov10635-vc-2",
        "ov10635-vc-3",
        "ov10635-vc-4",
        "ov10640-vc",
        "ov10640-vc-2",
        "ov10640-vc-3",
        "ov10640-vc-4",
    };
     bool mCamMipiCapture;

    struct EnvInfo {
        string env;
        string name;
    };
    vector <EnvInfo> mEnvInfo;

    struct VcInfo {
        int id;
        string name;
        int sequence;
        int total_num;
    };
    vector <VcInfo> mVcInfo;
};

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1080p_UYVY)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720p_UYVY)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x800_UYVY)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 1280;
    int height = 800;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720x576_UYVY)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_vga_UYVY)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_UYVY;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1080p_YUYV)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720p_YUYV)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x800_YUYV)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 1280;
    int height = 800;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720x576_YUYV)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_vga_YUYV)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_YUYV;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

// for RGB888 which is aligned by 32 bit
TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1080p_XBGR32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

// for RGB888 which is aligned by 32 bit
TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720p_XBGR32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

// for RGB888 which is aligned by 32 bit
TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x800_XBGR32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 1280;
    int height = 800;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

// for RGB888 which is aligned by 32 bit
TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720x576_XBGR32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

// for RGB888 which is aligned by 32 bit
TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_vga_XBGR32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_XBGR32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

// for RGB565 which is aligned by 32 bit
TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1080p_XRGB32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720p_XRGB32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x800_XRGB32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 1280;
    int height = 800;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720x576_XRGB32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_vga_XRGB32)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_XRGB32;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1080p_NV16)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_1080P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720p_NV16)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_720P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x800_NV16)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 1280;
    int height = 800;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_720x576_NV16)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = 720;
    int height = 576;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_vga_NV16)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_VGA_WIDTH;
    int height = RESOLUTION_VGA_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV16;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x1080_NV12)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_NV12;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}

TEST_F(camHalVirtualChannelTest, vc_qbuf_dqbuf_1280x1080_SRGGB12)
{
    if (camHalVirtualChannelTest::needSkipTest()) {
        LOGD("@%s, skip test!", __func__);
        return;
    }

    int width = RESOLUTION_720P_WIDTH;
    int height = RESOLUTION_1080P_HEIGHT;
    int fmt = V4L2_PIX_FMT_SRGGB12;
    int field = V4L2_FIELD_ANY;
    vector<struct TestInfo> infos = {
        {
            {100, 100, 100, 100},
            {0, 0, 0, 0}
        },
    };

    camHalVirtualChannelTest::main_test(width, height, fmt, field, infos);
}
