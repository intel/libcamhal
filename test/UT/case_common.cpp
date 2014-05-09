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

#define LOG_TAG "CASE_COMMON"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/v4l2-subdev.h>

#include "iutils/CameraLog.h"
#include "iutils/CameraDump.h"
#include "iutils/Utils.h"
#include "iutils/SwImageConverter.h"

#include "ICamera.h"
#include "PlatformData.h"
#include "Parameters.h"
#include "case_common.h"

using namespace icamera;

/**
 * Get camera name which uses tpg as default if not provided.
 * Return camera name which is never NULL.
 */
const char* getCurrentCameraName()
{
    const char* CAMERA_INPUT = "cameraInput";
    const char *input = getenv(CAMERA_INPUT);
    if (!input) {
        input = "tpg";
    }

    return input;
}

int getCurrentCameraId()
{
    int cameraId = 0;
    const char* input = getCurrentCameraName();
    int count = get_number_of_cameras();
    int id = 0;
    for (; id < count; id++) {
        camera_info_t info;
        CLEAR(info);
        get_camera_info(id, info);
        if (strcmp(info.name, input) == 0) {
            cameraId = id;
            break;
        }
    }

    EXPECT_NE(id, count);
    if (id == count) {
        LOGE("No camera name matched, please check if cameraInput is correct.");
    }

    LOGD("Camera (%s) id %d is used.", input, cameraId);
    return cameraId;
}

int getRandomValue(int min, int max)
{
    struct timeval tv;
    gettimeofday(&tv, NULL);
    srand((unsigned)tv.tv_usec);
    return (rand() % (max + 1 - min)) + min;
}

static int check_rgb(unsigned char *buffer, unsigned int width, unsigned int height)
{
    int x, y;
    int cr_cnt, cg_cnt, cb_cnt, gray_cnt;
    int over_thresh;
    int R, G, B;
    unsigned int diff0, diff1, diff2;

    cg_cnt = cr_cnt = cb_cnt = gray_cnt = 0;
    over_thresh = OVER_THRESH(height * width);

    for (y = 0; y < width; y++)
        for (x = 0; x < height; x++) {
            R = buffer[y * width + x];
            G = (buffer[y * width + x + 1] + buffer[(y + 1) * width + x]) >> 1;
            B = buffer[(y + 1) * width + x + 1];

            /* check over green */
            cr_cnt += (((R - G) > COLOR_THRESH_H) && ((R - B) >  COLOR_THRESH_H))?1:0;
            cg_cnt += (((G - R) > COLOR_THRESH_H) && ((G - B) >  COLOR_THRESH_H))?1:0;
            cb_cnt += (((B - R) > COLOR_THRESH_H) && ((B - G) >  COLOR_THRESH_H))?1:0;

            /* check black/white */
            diff0 = abs(R - G);
            diff1 = abs(R - B);
            diff2 = abs(G - B);

            if ( MAX3(diff0, diff1, diff2) < DIFF_THRESH)
                gray_cnt++;
        }

    if(cr_cnt > over_thresh)
        return OVER_RED;
    if(cg_cnt > over_thresh)
        return OVER_GREEN;
    if(cb_cnt > over_thresh)
        return OVER_BLUE;
    if(gray_cnt > over_thresh)
        return BLACK_WHITE;

    return NORMAL;
}

int check_image(int cameraId, void *data, unsigned int width,
                    unsigned int height, unsigned int length, unsigned int fmt)
{
    char *env = getenv("cameraImageCheck");
    if (!env || strcmp(env, "on")) {
        return 0;
    }

    int ret;
    int buf_size;
    unsigned char *buffer;

    buf_size = width * height << 2;
    buffer = new unsigned char[sizeof(char) * buf_size];
    memset(buffer, '\0', buf_size);

    ret = SwImageConverter::convertFormat(width, height, (unsigned char *)data, length, fmt,
                                     buffer, buf_size, V4L2_PIX_FMT_SRGGB8);
    if (ret) {
        printf("Failed to convert image format, ignoring corruption checking\n");
        delete [] buffer;
        return -1;
    }

    ret = check_rgb(buffer, width, height);
    switch(ret) {
        case NORMAL:
            printf("No corruption is found.\n");
            break;
        case OVER_RED:
            printf(FONT_COLOR_YELLOW "\nThe image should be corrupted with over RED!\n" COLOR_NONE);
            break;
        case OVER_GREEN:
            printf(FONT_COLOR_YELLOW "\nThe image should be corrupted with over GREEN!\n" COLOR_NONE);
            break;
        case OVER_BLUE:
            printf(FONT_COLOR_YELLOW "\nThe image should be corrupted with over BLUE!\n" COLOR_NONE);
            break;
        case BLACK_WHITE:
            printf(FONT_COLOR_YELLOW "\nThe image should be corrupted with monochrome!\n" COLOR_NONE);
            break;
        default:
            printf("Failed to check file!\n");
            break;
    }

    delete [] buffer;

    if (strcmp(getCurrentCameraName(), "tpg")) {
        return ret;
    }

    return 0;
}

stream_t getStreamByConfig(const supported_stream_config_t& config)
{
    stream_t stream;
    CLEAR(stream);
    stream.format = config.format;
    stream.width = config.width;
    stream.height = config.height;
    stream.field = config.field;
    stream.stride = config.stride;
    stream.size = config.size;
    return stream;
}

int getISysSupportedStreamConfig(stream_array_t& config)
{
    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t allConfigs;
    stream_t stream;
    info.capability->getSupportedStreamConfig(allConfigs);
    EXPECT_TRUE(allConfigs.size() > 0);

    for (int i = 0; i < allConfigs.size(); i++) {
        if (PlatformData::isISysSupportedFormat(cameraId, allConfigs[i].format)) {
            stream = getStreamByConfig(allConfigs[i]);
            config.push_back(stream);
        }
    }

    return OK;
}

bool isFeatureSupported(camera_features feature)
{
    camera_hal_init();
    int cameraId = getCurrentCameraId();
    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    camera_features_list_t features;
    info.capability->getSupportedFeatures(features);
    camera_hal_deinit();
    return std::find(features.begin(), features.end(), feature) != features.end();
}

stream_t camera_device_config_stream_normal(int cameraId, stream_t &config, int memType)
{
    // Configure streams
    stream_config_t stream_list;
    stream_t streams[1];
    streams[0] = config;
    streams[0].memType = memType;
    stream_list.num_streams = 1;
    stream_list.streams = streams;
    stream_list.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    int ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(streams[0].id, 0);
    return streams[0];
}

void camhal_qbuf_dqbuf_common(int width, int height, int fmt, int alloc_buffer_count, int dq_buffer_count,
                              int field, ParamList* params)
{
    /* init camera device */
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    camhal_qbuf_dqbuf(getCurrentCameraId(), width, height, fmt, alloc_buffer_count, dq_buffer_count, field, params);

    /* deinit camera device */
    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);

    SUCCEED();
}

void camhal_qbuf_dqbuf(int cameraId, int width, int height, int fmt, int alloc_buffer_count, int dq_buffer_count,
                              int field, ParamList* params, int total_virtual_channel_camera_num)
{
    camera_info_t info;
    int ret = get_camera_info(cameraId, info);
    LOGD("@%s, cameraId:%d, width:%d, height:%d, fmt:%s, field:%d",
            __func__, cameraId, width, height, CameraUtils::format2string(fmt), field);
    LOGD("@%s, alloc_buffer_count:%d, dq_buffer_count:%d, total_virtual_channel_camera_num:%d",
            __func__, alloc_buffer_count, dq_buffer_count, total_virtual_channel_camera_num);

    supported_stream_config_array_t configs;
    stream_t config;
    info.capability->getSupportedStreamConfig(configs);
    bool foundConfig = false;
    for (size_t i = 0; i < configs.size(); i++) {
        if (configs[i].field == field && configs[i].format == fmt
                && configs[i].width == width && configs[i].height == height) {
            foundConfig = true;
            config = getStreamByConfig(configs[i]);
            break;
        }
    }
    if (!foundConfig) {
        // Not fail here, since incorrect config case already covered in another case
        LOGD("Skip test for format:%s (%dx%d) field=%d",
                CameraUtils::pixelCode2String(fmt), width, height, field);
        camera_hal_deinit();
        return;
    }

    ret = camera_device_open(cameraId, total_virtual_channel_camera_num);
    EXPECT_EQ(ret, 0);

    // Check if need set parameter at beginning
    if (params != NULL && params->find(0) != params->end()) {
        Parameters& p = params->at(0);
        ret = camera_set_parameters(cameraId, p);
        EXPECT_EQ(OK, ret);
    }

    // Configure streams
    stream_t stream = camera_device_config_stream_normal(cameraId, config, V4L2_MEMORY_USERPTR);
    int stream_id = stream.id;
    EXPECT_EQ(stream_id, 0);

    /* allocate buffer */
    int page_size = getpagesize();
    camera_buffer_t *buffer;
    camera_buffer_t buffers[alloc_buffer_count];

    for (int i = 0; i < alloc_buffer_count; i++) {
        CLEAR(buffers[i]);
        buffer = &buffers[i];
        buffer->s = stream;
        EXPECT_TRUE(buffer->s.size > 0);

        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));
    }

    for (int i = 0; i < alloc_buffer_count; i++) {
        buffer = &buffers[i];
        ret = camera_stream_qbuf(cameraId, &buffer);
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < dq_buffer_count; i++) {
        ret = camera_stream_dqbuf(cameraId, stream_id, &buffer);
        EXPECT_EQ(ret, 0);
        if (ret != 0) {
            // Case failed already, no need to dqbuf again and again, and once dqduf
            // failed, it's unlikely to recover.
            break;
        }
        dumpImageBuffer(cameraId, *buffer);

        ret = check_image(cameraId, buffer->addr, buffer->s.width, buffer->s.height,
                              buffer->s.size, buffer->s.format);
        EXPECT_EQ(ret, 0);

        if (params != NULL && params->find(i + 1) != params->end()) {
            Parameters& p = params->at(i + 1);
            ret = camera_set_parameters(cameraId, p);
            EXPECT_EQ(OK, ret);
        }

        // Need to queue buf again when allocated buffer not enough for dequeuing.
        if (dq_buffer_count > alloc_buffer_count) {
            ret = camera_stream_qbuf(cameraId, &buffer);
            EXPECT_EQ(ret, 0);
        }
    }
    ret = camera_device_stop(cameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(cameraId);

    for (int i = 0; i < alloc_buffer_count; i++) {
        free(buffers[i].addr);
    }
}

void dumpImageBuffer(int cameraId, const camera_buffer_t& buffer)
{
    if (CameraDump::isDumpTypeEnable(DUMP_UT_BUFFER)) {
        BinParam_t binParam;
        binParam.bType = BIN_TYPE_BUFFER;
        binParam.mType = M_NA;
        binParam.sequence       = buffer.sequence;
        binParam.bParam.width   = buffer.s.width;
        binParam.bParam.height  = buffer.s.height;
        binParam.bParam.format  = buffer.s.format;
        CameraDump::dumpBinary(cameraId, buffer.addr, buffer.s.size, &binParam);
    }
}

int prepareStillStreams(int cameraId, stream_t streams[], int count)
{
    if (count > 0) {
        streams[0].width = 3264;
        streams[0].height = 2448;
        streams[0].format = V4L2_PIX_FMT_NV12;
        streams[0].field = V4L2_FIELD_ANY;
        streams[0].memType = V4L2_MEMORY_USERPTR;
        streams[0].usage = CAMERA_STREAM_STILL_CAPTURE;
    }
    if (count > 1) {
        streams[1].width = 384;
        streams[1].height = 288;
        streams[1].format = V4L2_PIX_FMT_NV12;
        streams[1].field = V4L2_FIELD_ANY;
        streams[1].memType = V4L2_MEMORY_USERPTR;
        streams[1].usage = CAMERA_STREAM_STILL_CAPTURE;
    }

    for (int i = 0; i < count; i++) {
        if (!PlatformData::isSupportedStream(cameraId, streams[i])) {
            return -1;
        }
    }

    return 0;
}

int prepareStreams(int cameraId, stream_t streams[], int count)
{
    for (int i = 0; i < count; i++) {
        streams[i].width = 1920;
        streams[i].height = 1080;
        streams[i].format = V4L2_PIX_FMT_NV12;
        streams[i].field = V4L2_FIELD_ANY;
        streams[i].memType = V4L2_MEMORY_USERPTR;
        streams[i].usage = CAMERA_STREAM_PREVIEW;
    }

    for (int i = 0; i < count; i++) {
        if (!PlatformData::isSupportedStream(cameraId, streams[i])) {
            return -1;
        }
    }

    return 0;
}

void test_configure_with_input_format(int inputFmt, int outputFmt, int width, int height)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    const int alloc_buffer_count = 8;
    const int dq_buffer_count = 16;

    stream_t config;
    CLEAR(config);
    config.format = outputFmt;
    config.width = width;
    config.height = height;
    config.stride = CameraUtils::getStride(outputFmt, width);
    config.size = CameraUtils::getFrameSize(outputFmt, width, height);
    if (!PlatformData::isSupportedStream(cameraId, config)) {
        // Not fail here, since incorrect config case already covered in another case
        LOGD("Skip test for format:%s (%dx%d) field=%d",
            CameraUtils::pixelCode2String(config.format), config.width, config.height, config.field);
        camera_hal_deinit();
        return;
    }

    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    // Configure streams
    stream_config_t stream_list;
    stream_t streams[1];
    streams[0] = config;
    streams[0].memType = V4L2_MEMORY_USERPTR;
    stream_list.num_streams = 1;
    stream_list.streams = streams;
    stream_list.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;

    stream_t input_config;
    CLEAR(input_config);
    input_config.format = inputFmt;
    if (!PlatformData::isISysSupportedFormat(cameraId, inputFmt)) {
        ret = camera_device_config_sensor_input(cameraId, &input_config);
        EXPECT_NE(ret, 0);
        ret = camera_device_config_streams(cameraId, &stream_list);
        EXPECT_NE(ret, 0);

        camera_device_close(cameraId);
        camera_hal_deinit();
        return;
    }
    ret = camera_device_config_sensor_input(cameraId, &input_config);
    EXPECT_EQ(ret, 0);
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(streams[0].id, 0);

    /* allocate buffer */
    int page_size = getpagesize();
    camera_buffer_t *buffer;
    camera_buffer_t buffers[alloc_buffer_count];

    for (int i = 0; i < alloc_buffer_count; i++) {
        CLEAR(buffers[i]);
        buffer = &buffers[i];
        buffer->s = streams[0];
        EXPECT_TRUE(buffer->s.size > 0);

        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));
    }

    for (int i = 0; i < alloc_buffer_count; i++) {
        buffer = &buffers[i];
        ret = camera_stream_qbuf(cameraId, &buffer);
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < dq_buffer_count; i++) {
        ret = camera_stream_dqbuf(cameraId, streams[0].id, &buffer);
        EXPECT_EQ(ret, 0);
        if (ret != 0) {
            // Case failed already, no need to dqbuf again and again, and once dqduf
            // failed, it's unlikely to recover.
            break;
        }
        dumpImageBuffer(cameraId, *buffer);

        // Need to queue buf again when allocated buffer not enough for dequeuing.
        if (dq_buffer_count > alloc_buffer_count) {
            ret = camera_stream_qbuf(cameraId, &buffer);
            EXPECT_EQ(ret, 0);
        }
    }
    ret = camera_device_stop(cameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(cameraId);

    for (int i = 0; i < alloc_buffer_count; i++) {
        free(buffers[i].addr);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

void test_configure_with_input_size(int inWidth, int inHeight, int outWidth, int outHeight)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    const int alloc_buffer_count = 8;
    const int dq_buffer_count = 16;

    stream_t config;
    CLEAR(config);
    config.format = V4L2_PIX_FMT_YUV420;
    config.width = outWidth;
    config.height = outHeight;
    config.stride = CameraUtils::getStride(V4L2_PIX_FMT_YUV420, outWidth);
    config.size = CameraUtils::getFrameSize(V4L2_PIX_FMT_YUV420, outWidth, outHeight);
    if (!PlatformData::isSupportedStream(cameraId, config)) {
        // Not fail here, since incorrect config case already covered in another case
        LOGD("Skip test for format:%s (%dx%d) field=%d",
            CameraUtils::pixelCode2String(config.format), config.width, config.height, config.field);
        camera_hal_deinit();
        return;
    }

    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    // Configure streams
    stream_config_t stream_list;
    stream_t streams[1];
    streams[0] = config;
    streams[0].memType = V4L2_MEMORY_USERPTR;
    stream_list.num_streams = 1;
    stream_list.streams = streams;
    stream_list.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;

    stream_t input_config;
    CLEAR(input_config);
    input_config.format = V4L2_PIX_FMT_SGRBG8V32;
    if (!PlatformData::isISysSupportedFormat(cameraId, V4L2_PIX_FMT_SGRBG8V32)) {
        ret = camera_device_config_sensor_input(cameraId, &input_config);
        EXPECT_NE(ret, 0);
        ret = camera_device_config_streams(cameraId, &stream_list);
        EXPECT_NE(ret, 0);

        camera_device_close(cameraId);
        camera_hal_deinit();
        return;
    }

    ret = camera_device_config_sensor_input(cameraId, &input_config);
    EXPECT_EQ(ret, 0);
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_EQ(ret, 0);
    EXPECT_EQ(streams[0].id, 0);

    /* allocate buffer */
    int page_size = getpagesize();
    camera_buffer_t *buffer;
    camera_buffer_t buffers[alloc_buffer_count];

    for (int i = 0; i < alloc_buffer_count; i++) {
        CLEAR(buffers[i]);
        buffer = &buffers[i];
        buffer->s = streams[0];
        EXPECT_TRUE(buffer->s.size > 0);

        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));
    }

    for (int i = 0; i < alloc_buffer_count; i++) {
        buffer = &buffers[i];
        ret = camera_stream_qbuf(cameraId, &buffer);
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < dq_buffer_count; i++) {
        ret = camera_stream_dqbuf(cameraId, streams[0].id, &buffer);
        EXPECT_EQ(ret, 0);
        if (ret != 0) {
            // Case failed already, no need to dqbuf again and again, and once dqduf
            // failed, it's unlikely to recover.
            break;
        }
        dumpImageBuffer(cameraId, *buffer);

        // Need to queue buf again when allocated buffer not enough for dequeuing.
        if (dq_buffer_count > alloc_buffer_count) {
            ret = camera_stream_qbuf(cameraId, &buffer);
            EXPECT_EQ(ret, 0);
        }
    }
    ret = camera_device_stop(cameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(cameraId);

    for (int i = 0; i < alloc_buffer_count; i++) {
        free(buffers[i].addr);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}
