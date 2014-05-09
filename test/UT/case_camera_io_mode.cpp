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
#include "MockSysCall.h"
#include "PlatformData.h"
#include "Parameters.h"
#include "case_common.h"

using namespace icamera;

static void camhal_io_mode_test_common(stream_array_t &configs, const int bufferCount, const int memType, const int bufferFlags)
{
    int cameraId = getCurrentCameraId();

    /* init camera device */
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    for (int i = 0; i < configs.size(); i++) {
        LOG2("%s iomode %d format %s %dx%d", __func__, configs[i].memType, CameraUtils::pixelCode2String(configs[i].format), configs[i].width, configs[i].height);
        ret = camera_device_open(cameraId);
        EXPECT_EQ(ret, 0);

        stream_t stream = camera_device_config_stream_normal(cameraId, configs[i], memType);
        int streamId = stream.id;
        EXPECT_EQ(streamId, 0);

        /* allocate buffer */
        camera_buffer_t *buffer;
        camera_buffer_t buffers[bufferCount];

        int j = 0;
        for (j = 0, buffer = buffers; j < bufferCount; j++, buffer++) {
            memset(buffer, 0, sizeof(camera_buffer_t));
            buffer->s = stream;
            EXPECT_TRUE(buffer->s.size > 0);
            buffer->flags = bufferFlags;

            ret = camera_device_allocate_memory(cameraId, buffer);
            EXPECT_EQ(ret, 0);

            if (buffer->flags & BUFFER_FLAG_DMA_EXPORT) {
                EXPECT_TRUE(buffer->dmafd > 0);
            } else {
                ret = camera_stream_qbuf(cameraId, &buffer);
                EXPECT_EQ(ret, 0);
            }
        }

        if (!(bufferFlags & BUFFER_FLAG_DMA_EXPORT)) {
            ret = camera_device_start(cameraId);
            EXPECT_EQ(ret, 0);

            /* dq double count of bufferCount  */
            for (j = 0; j < bufferCount*2; j++) {
                ret = camera_stream_dqbuf(cameraId, streamId, &buffer);
                EXPECT_EQ(ret, 0);

                ret = check_image(cameraId, buffer->addr, buffer->s.width, buffer->s.height,
                                    buffer->s.size, buffer->s.format);
                EXPECT_EQ(ret, 0);

                ret = camera_stream_qbuf(cameraId, &buffer);
                EXPECT_EQ(ret, 0);
            }

            ret = camera_device_stop(cameraId);
            EXPECT_EQ(ret, 0);
        }

        camera_device_close(cameraId);
    }

    /* deinit camera device */
    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);

    SUCCEED();
}

#ifndef MOCK_TEST
//Do not test this case in mock.
TEST_F(camHalTest, camera_device_io_mode_mmap)
{
    stream_array_t configs;
    getISysSupportedStreamConfig(configs);

    camhal_io_mode_test_common(configs, 8, V4L2_MEMORY_MMAP, 0);
}
#endif

TEST_F(camHalTest, camera_device_io_mode_dma)
{
    stream_array_t configs;
    getISysSupportedStreamConfig(configs);

    camhal_io_mode_test_common(configs, 8, V4L2_MEMORY_MMAP, BUFFER_FLAG_DMA_EXPORT);
}

