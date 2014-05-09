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

#define LOG_TAG "CASE_STREAM_OPS"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "ICamera.h"
#include "PlatformData.h"
#include "MockSysCall.h"
#include "Parameters.h"
#include "case_common.h"

TEST_F(camHalTest, camera_device_config_streams_normal)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    for (int i = 0; i < configs.size(); i++) {
        LOGD("Camera id:%d name:%s format:%s, resolution (%dx%d) field:%d.", cameraId, info.name,
                CameraUtils::pixelCode2String(configs[i].format),
                configs[i].width, configs[i].height, configs[i].field);
        ret = camera_device_open(cameraId);
        EXPECT_EQ(ret, 0);
        stream_t stream = getStreamByConfig(configs[i]);
        camera_device_config_stream_normal(cameraId, stream, V4L2_MEMORY_USERPTR);
        camera_device_close(cameraId);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_config_streams_invalid_param)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);
    int format = configs[0].format;
    EXPECT_TRUE(format != 0 && format != -1);

    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    // Configure streams
    int stream_id = -1;
    stream_config_t stream_list;
    stream_t streams[1];
    streams[0].width = 1920;
    streams[0].height = 1080;
    streams[0].format = format;
    streams[0].field = V4L2_FIELD_ANY;
    streams[0].memType = V4L2_MEMORY_USERPTR;
    stream_list.num_streams = 1;
    stream_list.streams = streams;
    stream_list.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    ret = camera_device_config_streams(cameraId,  NULL);
    EXPECT_TRUE(ret < 0);

    streams[0].width = 0;
    streams[0].height = 1080;
    streams[0].format = format;
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_TRUE(ret < 0);

    streams[0].width = 1920;
    streams[0].height = 0;
    streams[0].format = format;
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_TRUE(ret < 0);

    streams[0].width = 1920;
    streams[0].height = 1080;
    streams[0].format = -1;
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_TRUE(ret < 0);

    streams[0].width = 1234; // Invalid size not listed in config file
    streams[0].height = 421;
    streams[0].format = format;
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_TRUE(ret < 0);

    streams[0].width = 1920;
    streams[0].height = 1080;
    streams[0].format = 413413; // Invalid format not listed in config file
    ret = camera_device_config_streams(cameraId, &stream_list);
    EXPECT_TRUE(ret < 0);

    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

void configStreamAndQbuf(int cameraId, stream_t &config, camera_buffer_t *qbuf)
{
    stream_t stream = camera_device_config_stream_normal(cameraId, config, V4L2_MEMORY_USERPTR);
    int stream_id = stream.id;
    EXPECT_EQ(stream_id, 0);

    CLEAR(*qbuf);
    qbuf->s = stream;
    EXPECT_TRUE(qbuf->s.size > 0);

    int ret = posix_memalign(&qbuf->addr, getpagesize(), qbuf->s.size);
    EXPECT_TRUE((qbuf->addr != NULL) && (ret == 0));

    ret = camera_stream_qbuf(cameraId, &qbuf);
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_start_stop_normal)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    stream_t stream;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);
    for (int i = 0; i < configs.size(); i++) {
        LOGD("Camera id:%d name:%s current format:%s, resolution (%dx%d) field:%d", cameraId, info.name,
            CameraUtils::pixelCode2String(configs[i].format), configs[i].width, configs[i].height, configs[i].field);

        if (configs[i].field == V4L2_FIELD_ALTERNATE) {
            LOGD("skip interlaced format for now.");
            continue;
        }
        ret = camera_device_open(cameraId);
        EXPECT_EQ(ret, 0);

        // Configure streams and qbuf
        camera_buffer_t qbuf;
        camera_buffer_t *buf;
        stream = getStreamByConfig(configs[i]);
        configStreamAndQbuf(cameraId, stream, &qbuf);

        ret = camera_device_start(cameraId);
        EXPECT_EQ(ret, 0);

        int stream_id = qbuf.s.id;
        ret = camera_stream_dqbuf(cameraId, stream_id, &buf);
        EXPECT_EQ(ret, 0);

        ret = check_image(cameraId, buf->addr, buf->s.width, buf->s.height, buf->s.size, buf->s.format);
        EXPECT_EQ(ret, 0);

        ret = camera_device_stop(cameraId);
        EXPECT_EQ(ret, 0);

        // stop without dqbuf
        ret = camera_stream_qbuf(cameraId, &buf);
        EXPECT_EQ(ret, 0);

        ret = camera_device_start(cameraId);
        EXPECT_EQ(ret, 0);

        ret = camera_device_stop(cameraId);
        EXPECT_EQ(ret, 0);

        camera_device_close(cameraId);

        if (qbuf.addr) free(qbuf.addr);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_start_twice_stop_once)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    // Configure streams and qbuf
    camera_buffer_t qbuf;
    camera_buffer_t *buf;
    stream_t stream = getStreamByConfig(configs[0]);
    configStreamAndQbuf(cameraId, stream, &qbuf);

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    /* start twice */
    ret = camera_device_start(cameraId);
    EXPECT_NE(ret, 0);

    int stream_id = qbuf.s.id;
    ret = camera_stream_dqbuf(cameraId, stream_id, &buf);
    EXPECT_EQ(ret, 0);

    ret = check_image(cameraId, buf->addr, buf->s.width, buf->s.height, buf->s.size, buf->s.format);
    EXPECT_EQ(ret, 0);

    ret = camera_device_stop(cameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);

    if (qbuf.addr) free(qbuf.addr);
}

TEST_F(camHalTest, camera_device_start_without_add_stream)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    ret = camera_device_start(cameraId);
    EXPECT_NE(ret, 0);

    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_start_without_qbuf)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);

    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    stream_t stream;
    for (int i = 0; i < configs.size(); i++) {
        LOGD("Camera id:%d name:%s current format:%s, resolution (%dx%d) field:%d\n", cameraId, info.name,
            CameraUtils::pixelCode2String(configs[i].format), configs[i].width, configs[i].height, configs[i].field);

        ret = camera_device_open(cameraId);
        EXPECT_EQ(ret, 0);

        // Configure streams
        stream = getStreamByConfig(configs[i]);
        camera_device_config_stream_normal(cameraId, stream, V4L2_MEMORY_USERPTR);

        ret = camera_device_start(cameraId);
        EXPECT_NE(ret, 0);

        camera_device_close(cameraId);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_reconfig_streams_without_reopen)
{
    //TODO: Now just return direct until HSD 1504199533 bug is fixed.
    //This bug will cause dqbuf timeout when re-configure without close device.
    camera_hal_init();
    camera_hal_deinit();
    SUCCEED();
    return;

    int ret = camera_hal_init();
    const int bufferCount = 8;
    int j = 0;
    int page_size = getpagesize();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    if (configs.size() == 1) {
        LOG2("only one configs available, so skip this case");
        return ;
    }

    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    /* allocate buffer */
    camera_buffer_t *buffer;
    camera_buffer_t buffers[bufferCount];

    stream_t cfg;
    for (int i = 0; i < configs.size(); i++) {
        LOGD("Camera id:%d name:%s format:%s, resolution (%dx%d) field:%d.", cameraId, info.name,
                CameraUtils::pixelCode2String(configs[i].format),
                configs[i].width, configs[i].height, configs[i].field);

        cfg = getStreamByConfig(configs[i]);
        stream_t stream = camera_device_config_stream_normal(cameraId, cfg, V4L2_MEMORY_USERPTR);
        for (j = 0, buffer = buffers; j < bufferCount; j++, buffer++) {
            memset(buffer, 0, sizeof(camera_buffer_t));
            buffer->s = stream;
            EXPECT_TRUE(buffer->s.size > 0);

            ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
            EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

            ret = camera_stream_qbuf(cameraId, &buffer);
            EXPECT_EQ(ret, 0);
        }


        ret = camera_device_start(cameraId);
        EXPECT_EQ(ret, 0);

        for (int i = 0; i < bufferCount; i++) {
            buffer = &buffers[i];
            ret = camera_stream_dqbuf(cameraId, stream.id, &buffer);
            EXPECT_EQ(ret, 0);
            ret = camera_stream_qbuf(cameraId, &buffer);
            EXPECT_EQ(ret, 0);
        }

        camera_device_stop(cameraId);
        for (int i = 0; i < bufferCount; i++) {
            free(buffers[i].addr);
            buffers[i].addr = NULL;
        }
    }

    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

void camera_device_dual_streams_common(int cameraId, camera_buffer_t  **allBuffers, stream_config_t streamList)
{
    const int bufferNum = 2;
    int ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    ret = camera_device_config_streams(cameraId,  &streamList);

    const int page_size = getpagesize();
    const int bufferCount = 8;

    camera_buffer_t stream1buffers[bufferCount];
    camera_buffer_t stream2buffers[bufferCount];
    CLEAR(stream1buffers);
    CLEAR(stream2buffers);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t* buffer = &stream1buffers[i];
        stream_t stream = streamList.streams[0];
        buffer->s = stream;
        buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

        buffer = &stream2buffers[i];
        stream = streamList.streams[1];
        buffer->s = stream;
        buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

        allBuffers[0] = &stream1buffers[i];
        allBuffers[1] = &stream2buffers[i];

        ret = camera_stream_qbuf(cameraId, allBuffers, bufferNum);
        allBuffers[0] = NULL;
        allBuffers[1] = NULL;
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t *buffer = NULL;
        ret = camera_stream_dqbuf(cameraId, 0, &buffer);
        dumpImageBuffer(cameraId, *buffer);

        ret = camera_stream_dqbuf(cameraId, 1, &buffer);
        dumpImageBuffer(cameraId, *buffer);
    }

    camera_device_stop(cameraId);
    for (int i = 0; i < bufferCount; i++) {
        free(stream1buffers[i].addr);
        stream1buffers[i].addr = NULL;

        free(stream2buffers[i].addr);
        stream2buffers[i].addr = NULL;
    }

    camera_device_close(cameraId);
}

TEST_F(camHalTest, camera_device_dual_streams)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    int kTestFormat = V4L2_PIX_FMT_NV12;
    // Only test when PSYS is used.
    if(!PlatformData::usePsys(cameraId, kTestFormat)) {
        camera_hal_deinit();
        return;
    }

    camera_info_t info;
    const int kNumberOfStreams = 2;
    stream_t streams[kNumberOfStreams];
    stream_config_t streamList;
    streamList.num_streams = kNumberOfStreams;
    streamList.streams = streams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    camera_buffer_t *allBuffers[kNumberOfStreams] = {NULL, NULL};

    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    for (int i = 0; i < kNumberOfStreams; i++) {
        streams[i].field = V4L2_FIELD_ANY;
        streams[i].format = kTestFormat;
        streams[i].memType = V4L2_MEMORY_USERPTR;
    }
    for (int i = 0; i < configs.size(); i++) {
        if (configs[i].format != kTestFormat) continue;

        streams[0].width = configs[i].width;
        streams[0].height = configs[i].height;

        for (int j = 0; j < configs.size(); j++) {
            if (configs[j].format != kTestFormat) continue;

            streams[1].width = configs[j].width;
            streams[1].height = configs[j].height;
            LOGD("stream0: res: %dx%d, res: %dx%d",
                  streams[0].width, streams[0].height,
                  streams[1].width, streams[1].height);
            camera_device_dual_streams_common(cameraId, allBuffers, streamList);
        }
    }

    camera_hal_deinit();
}

TEST_F(camHalTest, camera_device_dual_streams_x3a)
{
    // Set image size to make sure the exact case can be tested.
    int width = 1920, height = 1088;

    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    int kTestFormat[2] = {V4L2_PIX_FMT_NV12, V4L2_PIX_FMT_SGRBG12};

    camera_info_t info;
    const int kNumberOfStreams = 2;
    stream_t streams[kNumberOfStreams];
    stream_config_t streamList;
    streamList.num_streams = kNumberOfStreams;
    streamList.streams = streams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    camera_buffer_t *allBuffers[kNumberOfStreams] = {NULL, NULL};

    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    for (int i = 0; i < kNumberOfStreams; i++) {
        streams[i].field = V4L2_FIELD_ANY;
        streams[i].format = kTestFormat[i];
        streams[i].memType = V4L2_MEMORY_USERPTR;
    }
    for (int i = 0; i < configs.size(); i++) {
        if ((configs[i].format != kTestFormat[0]) ||
            (configs[i].width != width) ||
            (configs[i].height != height)) {
            continue;
        }

        streams[0].width = configs[i].width;
        streams[0].height = configs[i].height;

        for (int j = 0; j < configs.size(); j++) {
            if ((configs[j].format != kTestFormat[1]) ||
                (configs[j].width != width) ||
                (configs[j].height != height)) {
                continue;
            }

            streams[1].width = configs[j].width;
            streams[1].height = configs[j].height;
            LOGD("stream0 res: %dx%d, format: %s, stream1 res: %dx%d, format: %s",
                  streams[0].width, streams[0].height, CameraUtils::format2string(kTestFormat[0]),
                  streams[1].width, streams[1].height, CameraUtils::format2string(kTestFormat[1]));
            camera_device_dual_streams_common(cameraId, allBuffers, streamList);
        }
    }

    camera_hal_deinit();
}

TEST_F(camHalTest, camera_device_configure_multi_streams_switch) {
    camera_hal_init();

    int cameraId = getCurrentCameraId();

    stream_t streams[2];
    stream_config_t streamList;
    streamList.streams = streams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;

    const int page_size = getpagesize();
    const int bufferNum = 1;
    const int bufferCount = 8;

    // one preview stream
    int numberOfStreams = 1;
    streamList.num_streams = numberOfStreams;
    if (prepareStreams(cameraId, streams, numberOfStreams) != 0) {
        camera_hal_deinit();
        return;
    }

    int ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    ret = camera_device_config_streams(cameraId,  &streamList);
    EXPECT_EQ(ret, 0);

    camera_buffer_t streambuffers[bufferCount];
    CLEAR(streambuffers);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t* buffer = &streambuffers[i];
        stream_t stream = streamList.streams[0];
        buffer->s = stream;
        buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

        ret = camera_stream_qbuf(cameraId, &buffer, bufferNum);
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t *buffer = NULL;
        ret = camera_stream_dqbuf(cameraId, 0, &buffer);
        dumpImageBuffer(cameraId, *buffer);
    }

    camera_device_stop(cameraId);
    for (int i = 0; i < bufferCount; i++) {
        free(streambuffers[i].addr);
        streambuffers[i].addr = NULL;
    }

    // still and thumbnail streams
    numberOfStreams = 2;
    streamList.num_streams = numberOfStreams;
    if (prepareStillStreams(cameraId, streams, numberOfStreams) == 0) {
        ret = camera_device_config_streams(cameraId,  &streamList);
        EXPECT_EQ(ret, 0);

        for (int i = 0; i < bufferCount; i++) {
            camera_buffer_t* buffer = &streambuffers[i];
            stream_t stream = streamList.streams[0];
            buffer->s = stream;
            buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
            ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
            EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

            ret = camera_stream_qbuf(cameraId, &buffer, bufferNum);
            EXPECT_EQ(ret, 0);
        }

        ret = camera_device_start(cameraId);
        EXPECT_EQ(ret, 0);

        for (int i = 0; i < bufferCount; i++) {
            camera_buffer_t *buffer = NULL;
            ret = camera_stream_dqbuf(cameraId, 0, &buffer);
            dumpImageBuffer(cameraId, *buffer);
        }

        camera_device_stop(cameraId);
        for (int i = 0; i < bufferCount; i++) {
            free(streambuffers[i].addr);
            streambuffers[i].addr = NULL;
        }
    }

    // one preview stream
    numberOfStreams = 1;
    streamList.num_streams = numberOfStreams;
    if (prepareStreams(cameraId, streams, numberOfStreams) != 0) {
        camera_hal_deinit();
        return;
    }

    ret = camera_device_config_streams(cameraId,  &streamList);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t* buffer = &streambuffers[i];
        stream_t stream = streamList.streams[0];
        buffer->s = stream;
        buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

        ret = camera_stream_qbuf(cameraId, &buffer, bufferNum);
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t *buffer = NULL;
        ret = camera_stream_dqbuf(cameraId, 0, &buffer);
        dumpImageBuffer(cameraId, *buffer);
    }

    camera_device_stop(cameraId);
    for (int i = 0; i < bufferCount; i++) {
        free(streambuffers[i].addr);
        streambuffers[i].addr = NULL;
    }

    camera_device_close(cameraId);
    camera_hal_deinit();
}

TEST_F(camHalTest, camera_device_configure_two_streams_queue_one_buffer) {
    camera_hal_init();

    int cameraId = getCurrentCameraId();

    const int numberOfStreams = 2;
    stream_t streams[numberOfStreams];
    stream_config_t streamList;
    streamList.num_streams = numberOfStreams;
    streamList.streams = streams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;

    if (prepareStreams(cameraId, streams, numberOfStreams) != 0) {
        camera_hal_deinit();
        return;
    }

    int ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    ret = camera_device_config_streams(cameraId,  &streamList);

    const int page_size = getpagesize();
    const int bufferNum = 1;
    const int bufferCount = 8;
    camera_buffer_t stream1buffers[bufferCount];
    CLEAR(stream1buffers);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t* buffer = &stream1buffers[i];
        stream_t stream = streamList.streams[0];
        buffer->s = stream;
        buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
        ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
        EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));

        ret = camera_stream_qbuf(cameraId, &buffer, bufferNum);
        EXPECT_EQ(ret, 0);
    }

    ret = camera_device_start(cameraId);
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < bufferCount; i++) {
        camera_buffer_t *buffer = NULL;
        ret = camera_stream_dqbuf(cameraId, 0, &buffer);
        dumpImageBuffer(cameraId, *buffer);
    }

    camera_device_stop(cameraId);
    for (int i = 0; i < bufferCount; i++) {
        free(stream1buffers[i].addr);
        stream1buffers[i].addr = NULL;
    }

    camera_device_close(cameraId);
    camera_hal_deinit();
}

TEST_F(camHalTest, camera_device_sdv_streams) {
    camera_hal_init();

    // The SDV case is only available for imx185 for now.
    if (strcmp(getCurrentCameraName(), "imx185") != 0) {
        camera_hal_deinit();
        return;
    }

    int cameraId = getCurrentCameraId();
    const int kTestFormat = V4L2_PIX_FMT_NV12;
    const int kNumberOfStreams = 2;
    stream_t streams[kNumberOfStreams];
    stream_config_t streamList;
    streamList.num_streams = kNumberOfStreams;
    streamList.streams = streams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_NORMAL;
    camera_buffer_t *allBuffers[kNumberOfStreams] = {nullptr, nullptr};

    for (int i = 0; i < kNumberOfStreams; i++) {
        streams[i].field = V4L2_FIELD_ANY;
        streams[i].format = kTestFormat;
        streams[i].memType = V4L2_MEMORY_USERPTR;
    }

    streams[0].width = 1920;
    streams[0].height = 1080;
    streams[0].usage = CAMERA_STREAM_PREVIEW;

    streams[1].width = 1280;
    streams[1].height = 720;
    streams[1].usage = CAMERA_STREAM_STILL_CAPTURE;

    camera_device_dual_streams_common(cameraId, allBuffers, streamList);

    camera_hal_deinit();
}

// FILE_SOURCE_S
TEST_F(camHalTest, camera_stream_producer_file_source) {
    camera_hal_init();

    const char* cameraName = getCurrentCameraName();
    const char* inputFile = "res/file_src_input_320_240.GRBG8V32";
    struct stat statBuf;

    // The case is only available for tpg.
    // And skip the test if the required input file doesn't exist.
    if (strcmp(cameraName, "tpg") != 0 || stat(inputFile, &statBuf) != 0) {
        camera_hal_deinit();
        LOGD("Skip the test due to unsupported sensor(%s) or missing input file(%s).", cameraName, inputFile);
        return;
    }

    // Enable the file injection mode.
    const char* PROP_CAMERA_FILE_INJECTION = "cameraInjectFile";
    const char* injectedFile = getenv(PROP_CAMERA_FILE_INJECTION);
    if (!injectedFile) {
        setenv(PROP_CAMERA_FILE_INJECTION, inputFile, 1);
    }

    int cameraId = getCurrentCameraId();
    camera_device_open(cameraId);

    // Configure streams and qbuf
    camera_buffer_t qbuf;
    camera_buffer_t *buf;
    stream_t config;
    CLEAR(config);
    config.width = 320;
    config.height = 240;
    config.format = V4L2_PIX_FMT_NV12;
    config.field = V4L2_FIELD_ANY;
    config.size = CameraUtils::getFrameSize(config.format, config.width, config.height);

    configStreamAndQbuf(cameraId, config, &qbuf);

    camera_device_start(cameraId);

    int stream_id = qbuf.s.id;
    const int kBufferCount = 10;
    for (int i = 0; i < kBufferCount; i++) {
        camera_stream_dqbuf(cameraId, stream_id, &buf);
        buf = &qbuf;
        camera_stream_qbuf(cameraId, &buf);
    }

    camera_stream_dqbuf(cameraId, stream_id, &buf);
    uint8_t* pBuf = (uint8_t*)buf->addr;
    // Randomly verify some of the values in the output buffer.
    EXPECT_EQ(0x10, pBuf[0]);
    EXPECT_EQ(0x3d, pBuf[0xd0]);
    EXPECT_EQ(0x47, pBuf[0x240]);
    EXPECT_EQ(0x42, pBuf[0x990]);
    EXPECT_EQ(0x3e, pBuf[0xd30]);

    camera_device_stop(cameraId);
    camera_device_close(cameraId);

    if (qbuf.addr) free(qbuf.addr);
    camera_hal_deinit();

    if (injectedFile) {
        setenv(PROP_CAMERA_FILE_INJECTION, injectedFile, 1);
    } else {
        unsetenv(PROP_CAMERA_FILE_INJECTION);
    }
}
// FILE_SOURCE_E

