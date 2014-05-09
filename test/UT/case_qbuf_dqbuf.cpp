/*
 * Copyright (C) 2015-2018 Intel Corporation.
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

#define LOG_TAG "CASE_BUFFER"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "linux/videodev2.h"
#include "iutils/Utils.h"
#include "iutils/CameraLog.h"
#include "PlatformData.h"
#include "ICamera.h"
#include "MockSysCall.h"
#include "case_common.h"

using namespace icamera;

TEST_F(camHalTest, camhal_qbuf_dqbuf_1080p_NV12_q8buffer_dq1buffer)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 1, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, camhal_qbuf_dqbuf_1080p_NV12_100_buffers)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 100, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, camhal_qbuf_dqbuf_1280x1080_SRGGB12_100_buffers)
{
    camhal_qbuf_dqbuf_common(1280, 1080, V4L2_PIX_FMT_SRGGB12, 8, 100, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, camhal_qbuf_dqbuf_all_supported_resolution_format)
{
    const int bufCnt = 8;
    int cameraId = getCurrentCameraId();
    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    for (int i = 0; i < configs.size(); i++) {
        if (i != 0) {
            // Manually SetUp if not first time run
            SetUp();
        }
        LOGD("Camera id:%d format:%s, resolution(%dx%d) type=%d\n", cameraId,
            CameraUtils::pixelCode2String(configs[i].format), configs[i].width, configs[i].height, configs[i].field);

        int format = configs[i].format;
        int width = configs[i].width;
        int height = configs[i].height;
        EXPECT_TRUE(width > 0);
        EXPECT_TRUE(height > 0);

        camhal_qbuf_dqbuf_common(width, height, format, bufCnt, bufCnt, configs[i].field);
    }
}

// Below cases only for debug mondello conveniently, they are actually already covered by
// camhal_qbuf_dqbuf_all_supported_resolution_format
// "mondello" which is both mipi and non mipi
TEST_F(camHalTest, mondello_qbuf_dqbuf_1080p_UYVY)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_720p_UYVY)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_800x480_UYVY)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_vga_UYVY)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_720x576_UYVY)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_1080i_UYVY)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_800x480_UYVY)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_576i_UYVY)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_480i_UYVY)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_UYVY, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_1080p_YUYV)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_720p_YUYV)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_vga_YUYV)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_720x576_YUYV)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_1080i_YUYV)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_576i_YUYV)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_interlaced_480i_YUYV)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_YUYV, 8, 8, V4L2_FIELD_ALTERNATE);
}

// "mondello-rgb8888" which is non mipi
TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_1080p_XBGR32)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_720p_XBGR32)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_vga_XBGR32)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_720x576_XBGR32)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_interlaced_1080i_XBGR32)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_interlaced_800x480_XBGR32)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_interlaced_576i_XBGR32)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb8888_qbuf_dqbuf_interlaced_480i_XBGR32)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_XBGR32, 8, 8, V4L2_FIELD_ALTERNATE);
}

// "mondello-rgb888" which is mipi
TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_1080p_BGR24)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_720p_BGR24)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_800x480_BGR24)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_vga_BGR24)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_720x576_BGR24)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_interlaced_1080i_BGR24)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_interlaced_576i_BGR24)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb888_qbuf_dqbuf_interlaced_480i_BGR24)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_BGR24, 8, 8, V4L2_FIELD_ALTERNATE);
}

// "mondello-rgb565-32bpp" which is non mipi
TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_1080p_XRGB32)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_720p_XRGB32)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_vga_XRGB32)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_720x576_XRGB32)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_interlaced_1080i_XRGB32)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_interlaced_800x480_XRGB32)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_interlaced_576i_XRGB32)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb565_32bpp_qbuf_dqbuf_interlaced_480i_XRGB32)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_XRGB32, 8, 8, V4L2_FIELD_ALTERNATE);
}

// "mondello-rgb565" which is mipi
TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_1080p_RGB565)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_720p_RGB565)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_800x480_RGB565)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_vga_RGB565)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_720x576_RGB565)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_interlaced_1080i_RGB565)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_interlaced_576i_RGB565)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_rgb565_qbuf_dqbuf_interlaced_480i_RGB565)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_RGB565, 8, 8, V4L2_FIELD_ALTERNATE);
}

// NV16
TEST_F(camHalTest, mondello_qbuf_dqbuf_1080p_NV16)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_720p_NV16)
{
    camhal_qbuf_dqbuf_common(1280, 720, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_800x480_NV16)
{
    camhal_qbuf_dqbuf_common(800, 480, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_vga_NV16)
{
    camhal_qbuf_dqbuf_common(640, 480, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_qbuf_dqbuf_720x576_NV16)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, mondello_nv16_qbuf_dqbuf_interlaced_1080i_NV16)
{
    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_nv16_qbuf_dqbuf_interlaced_576i_NV16)
{
    camhal_qbuf_dqbuf_common(720, 576, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, mondello_nv16_qbuf_dqbuf_interlaced_480i_NV16)
{
    camhal_qbuf_dqbuf_common(720, 480, V4L2_PIX_FMT_NV16, 8, 8, V4L2_FIELD_ALTERNATE);
}

TEST_F(camHalTest, ov10640_srggb12_qbuf_dqbuf_1280x1080)
{
    camhal_qbuf_dqbuf_common(1280, 1080, V4L2_PIX_FMT_SRGGB12, 8, 8, V4L2_FIELD_ANY);
}

TEST_F(camHalTest, ov10640_nv12_qbuf_dqbuf_1280x1080)
{
    camhal_qbuf_dqbuf_common(1280, 1080, V4L2_PIX_FMT_NV12, 8, 8, V4L2_FIELD_ANY);
}

