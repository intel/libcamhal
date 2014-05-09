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

#define LOG_TAG "CASE_STATIC_INFO"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <string>

#include "isp_control/IspControlUtils.h"
#include "iutils/Utils.h"
#include "iutils/CameraLog.h"
#include "ICamera.h"
#include "MockSysCall.h"
#include "Parameters.h"
#include "case_common.h"
#include "PlatformData.h"

using namespace icamera;

TEST(camHalRawTest, get_number_of_cameras_without_init) {
    int count = get_number_of_cameras();
    LOGD("Get cameras numbers %d.", count);
    EXPECT_TRUE(count > 0);
}

TEST_F(camHalTest, get_number_of_cameras_after_init) {
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();

    LOGD("Get cameras numbers %d.", count);
    EXPECT_TRUE(count > 0);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST(camHalRawTest, get_camera_info_check_all_field) {
    int count = get_number_of_cameras();
    for (int id = 0; id < count; id++) {
        camera_info_t info;
        info.facing = -1;
        info.orientation = -1;
        info.device_version = -1;
        info.name = NULL;
        info.capability = NULL;
        int ret = get_camera_info(id, info);
        EXPECT_EQ(OK, ret);
        EXPECT_NE(info.facing, -1);
        EXPECT_NE(info.orientation, -1);
        EXPECT_NE(info.device_version, -1);
        EXPECT_NOT_NULL(info.name);
        EXPECT_NOT_NULL(info.capability);
        LOGD("Camera id:%d sensor name: %s (%s)", id, info.name, info.description);

        if (info.capability) {
            vector<uint32_t> controls;
            info.capability->getSupportedIspControlFeatures(controls);
            for (auto ctrlId : controls) {
                LOGD("Supported ISP control:%s", IspControlUtils::getNameById(ctrlId));
            }
        }
    }
}

int getAndCheckCameraInfo(int cameraId) {
    camera_info_t info;
    CLEAR(info);
    int ret = get_camera_info(cameraId, info);
    if (ret != OK) {
        LOGE("Error... during get camera info ret=%d", ret);
        return ret;
    }
    if (info.capability == NULL) {
        LOGE("Error... no available capability info in camera info.");
        return UNKNOWN_ERROR;
    }

    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    if (configs.size() == 0) {
        LOGE("Error... camera info does not contain correct stream config info.");
        return UNKNOWN_ERROR;
    }
    camera_range_array_t fpsRanges;
    info.capability->getSupportedFpsRange(fpsRanges);
    if (fpsRanges.size() == 0) {
        LOGE("Error... camera info does not contain correct fps range info.");
        return UNKNOWN_ERROR;
    }
    return OK;
}

TEST(camHalRawTest, get_camera_info_with_invalid_camera_id) {
    int count = get_number_of_cameras();
    int ret = getAndCheckCameraInfo(count);
    EXPECT_NE(ret, OK);
}

TEST(camHalRawTest, get_camera_info_before_get_number_of_camera) {
    int ret = getAndCheckCameraInfo(0);
    int count = get_number_of_cameras();
    if (count > 0) {
        EXPECT_EQ(OK, ret);
    } else {
        EXPECT_NE(ret, OK);
    }
}

TEST(camHalRawTest, get_camera_info_without_init) {
    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);
    int ret = 0;
    for(int id = 0; id < count; id++) {
        ret = getAndCheckCameraInfo(id);
        EXPECT_EQ(OK, ret);
    }
}

TEST(camHalRawTest, get_camera_info_check_stream_config) {
    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);
    for(int id = 0; id < count; id++) {
        camera_info_t info;
        get_camera_info(id, info);
        supported_stream_config_array_t configs;
        info.capability->getSupportedStreamConfig(configs);
        EXPECT_NE(configs.size(), 0);
        for (size_t i = 0; i < configs.size(); i++) {
            EXPECT_TRUE(configs[i].format != 0 && configs[i].format != -1);
            EXPECT_TRUE(configs[i].width > 0 && configs[i].width < 10000);
            EXPECT_TRUE(configs[i].height > 0 && configs[i].height < 10000);
            EXPECT_TRUE(configs[i].stride >= configs[i].width);
            EXPECT_TRUE(configs[i].size > 0);
            // currently only type "any" and "alternate" are available
            EXPECT_TRUE(configs[i].field == V4L2_FIELD_ANY || configs[i].field == V4L2_FIELD_ALTERNATE);
            LOGD("Camera id:%d\tname:%s\tformat:%s\t(%dx%d)\tstride:%d\tbufSize:%d\tfield:%d\tfps:%d,%d",
                    id, info.name, CameraUtils::pixelCode2String(configs[i].format),
                    configs[i].width, configs[i].height, configs[i].stride, configs[i].size, configs[i].field,
                    configs[i].maxVideoFps, configs[i].maxCaptureFps);
        }
    }
}

TEST(camHalRawTest, get_camera_info_check_supported_features) {
    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);
    for (int id = 0; id < count; id++) {
        camera_info_t info;
        get_camera_info(id, info);
        camera_features_list_t features;
        info.capability->getSupportedFeatures(features);
        for(size_t i = 0; i < features.size(); i++) {
            EXPECT_TRUE(features[i] >= 0);
            EXPECT_TRUE(features[i] < camera_features::INVALID_FEATURE);
        }
    }
}


TEST(camHalRawTest, get_camera_info_name_matched) {
    int cameraId = getCurrentCameraId();
    camera_info_t info;
    CLEAR(info);
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.name);
    EXPECT_EQ(strcmp(info.name, getCurrentCameraName()), 0);
}

TEST_F(camHalTest, get_camera_info_after_hal_init) {
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);

    for(int id = 0; id < count; id++) {
        ret = getAndCheckCameraInfo(id);
        EXPECT_EQ(OK, ret);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, param_set_and_get) {
    int camera_id = 0;
    Parameters param;
    camera_range_t fps_set = {10, 30};
    param.setFpsRange(fps_set);

    int ret = camera_hal_init();
    EXPECT_EQ(OK, ret);
    ret = camera_device_open(camera_id);
    EXPECT_EQ(OK, ret);

    ret = camera_set_parameters(camera_id, param);
    EXPECT_EQ(OK, ret);

    Parameters param_get;
    ret = camera_get_parameters(camera_id, param_get);
    EXPECT_EQ(OK, ret);
    camera_range_t fps_get;
    param_get.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 10);
    EXPECT_EQ(fps_get.max, 30);

    // camera capabilities related parameters should be always included in
    supported_stream_config_array_t configs;
    param_get.getSupportedStreamConfig(configs);
    EXPECT_NE(configs.size(), 0);
    camera_range_array_t ranges;
    param_get.getSupportedFpsRange(ranges);
    EXPECT_NE(ranges.size(), 0);

    camera_device_close(camera_id);
    camera_hal_deinit();
}

TEST_F(camHalTest, param_get_default) {
    int camera_id = 0;
    camera_hal_init();
    camera_device_open(camera_id);

    Parameters param_get;
    int ret = camera_get_parameters(camera_id, param_get);
    EXPECT_EQ(OK, ret);

    camera_range_t fps_get;
    param_get.getFpsRange(fps_get);
    EXPECT_NE(fps_get.min, 0);
    EXPECT_NE(fps_get.max, 0);
    camera_device_close(camera_id);
    camera_hal_deinit();
}

/**
 * Test if default parameter beyond camera's capability
 */
TEST_F(camHalTest, param_default_is_supported) {
    int ret = OK;

    int camera_id = 0;
    camera_info_t info;
    CLEAR(info);
    ret = get_camera_info(camera_id, info);
    EXPECT_EQ(OK, ret);
    EXPECT_NOT_NULL(info.capability);
    camera_range_array_t ranges;
    ret = info.capability->getSupportedFpsRange(ranges);
    EXPECT_EQ(OK, ret);
    EXPECT_NE(ranges.size(), 0);

    camera_hal_init();
    camera_device_open(camera_id);

    Parameters param_get;
    ret = camera_get_parameters(camera_id, param_get);
    camera_range_t fps_get;
    param_get.getFpsRange(fps_get);
    bool found = false;
    for (size_t i = 0; i < ranges.size(); i++) {
        if (ranges[i].min == fps_get.min && ranges[i].max == fps_get.max) {
            found = true;
            break;
        }
    }
    EXPECT_TRUE(found);

    camera_device_close(camera_id);
    camera_hal_deinit();
}

// Meaning of each field: { Format, Width, Height, Field, Stride, Size}
// "field" here is unused, just use 0 as default.
stream_t STREAM_CONFIG_INFO[] = {
    {   V4L2_PIX_FMT_SGRBG8V32, 320,    240,    0   ,   640,    154624      },
    {   V4L2_PIX_FMT_SGRBG8V32, 640,    480,    0   ,   1280,   615680      },
    {   V4L2_PIX_FMT_SGRBG8V32, 1280,   720,    0   ,   2560,   1845760     },
    {   V4L2_PIX_FMT_SGRBG8V32, 1280,   800,    0   ,   2560,   2050560     },
    {   V4L2_PIX_FMT_SGRBG8V32, 1920,   1080,   0   ,   3840,   4151040     },

    {   V4L2_PIX_FMT_NV12,  176,    144,    0   ,   192,    42496       },
    {   V4L2_PIX_FMT_NV12,  240,    135,    0   ,   256,    52736       },
    {   V4L2_PIX_FMT_NV12,  240,    160,    0   ,   256,    62464       },
    {   V4L2_PIX_FMT_NV12,  320,    240,    0   ,   320,    116224      },
    {   V4L2_PIX_FMT_NV12,  384,    216,    0   ,   384,    125440      },
    {   V4L2_PIX_FMT_NV12,  384,    288,    0   ,   384,    166912      },
    {   V4L2_PIX_FMT_NV12,  640,    480,    0   ,   640,    461824      },
    {   V4L2_PIX_FMT_NV12,  720,    480,    0   ,   768,    554112      },
    {   V4L2_PIX_FMT_NV12,  720,    576,    0   ,   768,    664704      },
    {   V4L2_PIX_FMT_NV12,  1280,   720,    0   ,   1280,   1384320     },
    {   V4L2_PIX_FMT_NV12,  1280,   800,    0   ,   1280,   1537920     },
    {   V4L2_PIX_FMT_NV12,  1280,   1080,   0   ,   1280,   2075520     },
    {   V4L2_PIX_FMT_NV12,  1280,   1088,   0   ,   1280,   2090880     },
    {   V4L2_PIX_FMT_NV12,  1600,   1200,   0   ,   1600,   2882400     },
    {   V4L2_PIX_FMT_NV12,  1920,   1080,   0   ,   1920,   3113280     },
    {   V4L2_PIX_FMT_NV12,  1920,   1088,   0   ,   1920,   3136320     },
    {   V4L2_PIX_FMT_NV12,  1940,   1092,   0   ,   1984,   3287744     },
    {   V4L2_PIX_FMT_NV12,  3264,   2448,   0   ,   3264,   11990304    },
    {   V4L2_PIX_FMT_NV12,  3840,   2160,   0   ,   3840,   12447360    },
    {   V4L2_PIX_FMT_NV12,  4032,   3008,   0   ,   4032,   18198432    },

    {   V4L2_PIX_FMT_YUV420,1920,   1080,   0   ,   1920,   3113280     },
    {   V4L2_PIX_FMT_RGB24, 1920,   1080,   0   ,   5760,   6226560     },

    {   V4L2_PIX_FMT_NV16,  320,    240,    0   ,   320,    154624      },
    {   V4L2_PIX_FMT_NV16,  640,    480,    0   ,   640,    615680      },
    {   V4L2_PIX_FMT_NV16,  720,    480,    0   ,   768,    738816      },
    {   V4L2_PIX_FMT_NV16,  720,    576,    0   ,   768,    886272      },
    {   V4L2_PIX_FMT_NV16,  800,    480,    0   ,   832,    800384      },
    {   V4L2_PIX_FMT_NV16,  1280,   720,    0   ,   1280,   1845760     },
    {   V4L2_PIX_FMT_NV16,  1280,   800,    0   ,   1280,   2050560     },
    {   V4L2_PIX_FMT_NV16,  1920,   1080,   0   ,   1920,   4151040     },

    {   V4L2_PIX_FMT_YUYV,  320,    240,    0   ,   640,    154624      },
    {   V4L2_PIX_FMT_YUYV,  640,    480,    0   ,   1280,   615680      },
    {   V4L2_PIX_FMT_YUYV,  720,    480,    0   ,   1472,   708032      },
    {   V4L2_PIX_FMT_YUYV,  720,    576,    0   ,   1472,    849344      },
    {   V4L2_PIX_FMT_YUYV,  896,    480,    0   ,   1792,    861952      },
    {   V4L2_PIX_FMT_YUYV,  1280,   720,    0   ,   2560,   1845760     },
    {   V4L2_PIX_FMT_YUYV,  1280,   768,    0   ,   2560,   1968640     },
    {   V4L2_PIX_FMT_YUYV,  1280,   800,    0   ,   2560,   2050560     },
    {   V4L2_PIX_FMT_YUYV,  1280,   1080,   0   ,   2560,   2767360     },
    {   V4L2_PIX_FMT_YUYV,  1920,   1080,   0   ,   3840,   4151040     },
    {   V4L2_PIX_FMT_YUYV,  1920,   1088,   0   ,   3840,   4181760     },

    {   V4L2_PIX_FMT_SRGGB12,    1280,   1080,   0   ,   2560,   2767360     },
    {   V4L2_PIX_FMT_SRGGB12,    1932,   1094,   0   ,   3904,   4274880     },
    {   V4L2_PIX_FMT_SRGGB12,    3864,   2202,   0   ,   7744,   17060032     },

    {   V4L2_PIX_FMT_SRGGB10,    1932,   1094,    0   ,   3904,   4274880     },
    {   V4L2_PIX_FMT_SRGGB10,    3864,   2174,    0   ,   7744,   16843200     },
    {   V4L2_PIX_FMT_SRGGB10,    3868,   4448,    0   ,   7744,   34453056     },

    {   V4L2_PIX_FMT_SGRBG10,    3280,   2464,    0   ,   6592,   16249280     },

    {   V4L2_PIX_FMT_SGRBG10V32,    1920,   1080,   0   ,   3840,   4151040     },
    {   V4L2_PIX_FMT_SGRBG10V32,    3264,   2448,   0   ,   6528,   15987072    },
    {   V4L2_PIX_FMT_SGRBG12V32,    1920,   1080,   0   ,   3840,   4151040     },

    {   V4L2_PIX_FMT_BGR24, 240,    135,    0   ,   768,    104704      },
    {   V4L2_PIX_FMT_BGR24, 240,    160,    0   ,   768,    123904      },
    {   V4L2_PIX_FMT_BGR24, 640,    480,    0   ,   1920,    923520      },
    {   V4L2_PIX_FMT_BGR24, 720,    480,    0   ,   2176,    524416      },
    {   V4L2_PIX_FMT_BGR24, 720,    576,    0   ,   2176,    1255552     },
    {   V4L2_PIX_FMT_BGR24, 800,    480,    0   ,   2432,    1169792     },
    {   V4L2_PIX_FMT_BGR24, 1280,   720,    0   ,   3840,    2768640     },
    {   V4L2_PIX_FMT_BGR24, 1920,   1080,   0   ,   5760,    6226560     },
    {   V4L2_PIX_FMT_BGR24, 3840,   2160,   0   ,   11520,    24894720     },

    {   V4L2_PIX_FMT_RGB565,    240,    135,    0   ,   512,    70144     },
    {   V4L2_PIX_FMT_RGB565,    240,    160,    0   ,   512,    82944     },
    {   V4L2_PIX_FMT_RGB565,    640,    480,    0   ,   1280,    615680      },
    {   V4L2_PIX_FMT_RGB565,    720,    480,    0   ,   1472,    708032      },
    {   V4L2_PIX_FMT_RGB565,    720,    576,    0   ,   1472,    849344      },
    {   V4L2_PIX_FMT_RGB565,    800,    480,    0   ,   1600,    769600      },
    {   V4L2_PIX_FMT_RGB565,    1280,   720,    0   ,   2560,   1845760     },
    {   V4L2_PIX_FMT_RGB565,    1920,   1080,   0   ,   3840,   4151040     },
    {   V4L2_PIX_FMT_RGB565,    3840,   2160,   0   ,   7680,   16596480     },

    {   V4L2_PIX_FMT_UYVY,  640,    480,    0   ,   1280,    615680      },
    {   V4L2_PIX_FMT_UYVY,  720,    480,    0   ,   1472,    708032      },
    {   V4L2_PIX_FMT_UYVY,  720,    576,    0   ,   1472,    849344      },
    {   V4L2_PIX_FMT_UYVY,  800,    480,    0   ,   1600,    769600      },
    {   V4L2_PIX_FMT_UYVY,  896,    480,    0   ,   1792,    861952      },
    {   V4L2_PIX_FMT_UYVY,  1280,   720,    0   ,   2560,   1845760     },
    {   V4L2_PIX_FMT_UYVY,  1280,   768,    0   ,   2560,   1968640     },
    {   V4L2_PIX_FMT_UYVY,  1280,   800,    0   ,   2560,   2050560     },
    {   V4L2_PIX_FMT_UYVY,  1920,   1080,   0   ,   3840,   4151040     },
    {   V4L2_PIX_FMT_UYVY,  1920,   1088,   0   ,   3840,   4181760     },

    {   V4L2_PIX_FMT_BGR32,    640,    480,    0   ,   2560,    1231360     },
    {   V4L2_PIX_FMT_BGR32,    720,    480,    0   ,   2880,    1385280     },
    {   V4L2_PIX_FMT_BGR32,    720,    576,    0   ,   2880,    1661760     },
    {   V4L2_PIX_FMT_BGR32,    800,    480,    0   ,   3200,    1539200     },
    {   V4L2_PIX_FMT_BGR32,    1280,   720,    0   ,   5120,   3691520     },
    {   V4L2_PIX_FMT_BGR32,    1920,   1080,   0   ,   7680,   8302080     },
    {   V4L2_PIX_FMT_BGR32,    3840,   2160,   0   ,   15360,  33192960     },

    {   V4L2_PIX_FMT_XBGR32,    640,    480,    0   ,   2560,    1231360     },
    {   V4L2_PIX_FMT_XBGR32,    720,    480,    0   ,   2880,    1385280     },
    {   V4L2_PIX_FMT_XBGR32,    720,    576,    0   ,   2880,    1661760     },
    {   V4L2_PIX_FMT_XBGR32,    800,    480,    0   ,   3200,    1539200     },
    {   V4L2_PIX_FMT_XBGR32,    1280,   720,    0   ,   5120,   3691520     },
    {   V4L2_PIX_FMT_XBGR32,    1920,   1080,   0   ,   7680,   8302080     },
    {   V4L2_PIX_FMT_XBGR32,    3840,   2160,   0   ,   15360,  33192960     },

    {   V4L2_PIX_FMT_XRGB32,    640,    480,    0   ,   2560,    1231360     },
    {   V4L2_PIX_FMT_XRGB32,    720,    480,    0   ,   2880,    1385280     },
    {   V4L2_PIX_FMT_XRGB32,    720,    576,    0   ,   2880,    1661760     },
    {   V4L2_PIX_FMT_XRGB32,    800,    480,    0   ,   3200,    1539200     },
    {   V4L2_PIX_FMT_XRGB32,    1280,   720,    0   ,   5120,   3691520     },
    {   V4L2_PIX_FMT_XRGB32,    1920,   1080,   0   ,   7680,   8302080     },
};

TEST(camRawTest, check_get_frame_size_api) {
    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);
    for(int id = 0; id < count; id++) {
        camera_info_t info;
        get_camera_info(id, info);
        supported_stream_config_array_t configs;
        info.capability->getSupportedStreamConfig(configs);
        EXPECT_NE(configs.size(), 0);

        stream_t *configInfo = STREAM_CONFIG_INFO;
        int len = ARRAY_SIZE(STREAM_CONFIG_INFO);

        for (size_t i = 0; i < configs.size(); i++) {
            int j;
            for (j = 0; j < len; j++) {
                if (configs[i].format == configInfo[j].format &&
                    configs[i].width  == configInfo[j].width &&
                    configs[i].height == configInfo[j].height) {

                    EXPECT_EQ(configInfo[j].stride, configs[i].stride) <<"Format:"
                        << CameraUtils::pixelCode2String(configInfo[j].format)<<" Width:"<<configInfo[j].width<<
                        " Height:"<<configInfo[j].height<<" STRIDE unmatch:"<<configInfo[j].stride<<" vs. "
                        <<configs[i].stride;
                    EXPECT_EQ(configInfo[j].size, configs[i].size) <<"Format:"
                        << CameraUtils::pixelCode2String(configInfo[j].format)<<" Width:"<<configInfo[j].width<<
                        " Height:"<<configInfo[j].height<<" SIZE unmatch:"<<configInfo[j].size<<" vs. "
                        <<configs[i].size;
                    break;
                }
            }
            EXPECT_TRUE(j < len)<< "Format:"<<CameraUtils::pixelCode2String(configs[i].format)<<
                " Width:"<<configs[i].width<<" Height:"<<configs[i].height<<" doesn't exist in struct array.";
        }
    }
    PlatformData::releaseInstance();
}
