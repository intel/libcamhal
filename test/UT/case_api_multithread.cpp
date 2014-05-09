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

#define LOG_TAG "CASE_API_MULTI_THREAD"

#include <cstdlib>

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "ICamera.h"
#include "PlatformData.h"
#include "MockSysCall.h"
#include "case_common.h"

class BufferLoopThread : public Thread {
public:
    BufferLoopThread(int cameraId, const stream_t& stream) {
         mCameraId = cameraId;
         mStream = stream;
         mRetryCount = 10;

         CLEAR(mBuffers);
    }

    ~BufferLoopThread() {
        EXPECT_TRUE(verifyBufferSequence());
        releaseBuffers();
    }

    int allocateBuffers() {
        for (int i = 0; i < kBufPoolSize; i++) {
            camera_buffer_t* buffer = &mBuffers[i];
            buffer->s = mStream;

            int ret = posix_memalign(&buffer->addr, getpagesize(), buffer->s.size);
            if (buffer->addr == nullptr || ret != 0) {
                return NO_MEMORY;
            }
        }
        return OK;
    }

    void releaseBuffers() {
        for (int i = 0; i < kBufPoolSize; i++) {
            if (mBuffers[i].addr) {
                free(mBuffers[i].addr);
            }
        }
        CLEAR(mBuffers);
    }

    int queueOneBuffer(camera_buffer_t* buffer) {
        mAllQueuedBuffers.push_back(*buffer);
        return camera_stream_qbuf(mCameraId, &buffer);
    }

    int queueAllBuffers() {
        for (int i = 0; i < kBufPoolSize; i++) {
            queueOneBuffer(&mBuffers[i]);
        }
        return OK;
    }

    // Used to verify if the buffer loop is FIFO.
    bool verifyBufferSequence() {
        int totalBufferNum = mAllDequeuedBuffers.size();
        for (int index = 0; index < totalBufferNum; index++) {
            if (mAllQueuedBuffers[index].addr != mAllDequeuedBuffers[index].addr) {
                return false;
            }
        }

        return true;
    }

private:
    bool threadLoop() {
        camera_buffer_t* buffer = nullptr;
        int ret = camera_stream_dqbuf(mCameraId, mStream.id, &buffer);
        if (ret != 0 || buffer == nullptr) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            mRetryCount--;
            return mRetryCount >= 0;
        }

        mAllDequeuedBuffers.push_back(*buffer);

        queueOneBuffer(buffer);

        return true;
    }

private:
    static const int kBufPoolSize = 4;
    camera_buffer_t mBuffers[kBufPoolSize];

    int mCameraId;
    stream_t mStream;
    int mRetryCount;

    vector<camera_buffer_t> mAllQueuedBuffers;
    vector<camera_buffer_t> mAllDequeuedBuffers;
};

struct TestParam {
    int mCameraId;
    int mWidth;
    int mHeight;
    int mFormat;
    bool mStopBufferFirst;

    void dump() const {
        LOGD("TestParam: cameraId:%d, fmt:%s(%dx%d), stop buffer first:%d",
             mCameraId, CameraUtils::format2string(mFormat), mWidth, mHeight, mStopBufferFirst);
    }
};

void mutil_thread_buffer_loop_common(const TestParam& param)
{
    param.dump();

    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    camera_info_t info;
    ret = get_camera_info(param.mCameraId, info);

    supported_stream_config_array_t configs;
    stream_t config;
    info.capability->getSupportedStreamConfig(configs);
    bool foundConfig = false;
    for (size_t i = 0; i < configs.size(); i++) {
        if (configs[i].format == param.mFormat && configs[i].width == param.mWidth &&
                configs[i].height == param.mHeight) {
            foundConfig = true;
            config = getStreamByConfig(configs[i]);
            break;
        }
    }
    if (!foundConfig) {
        LOGD("Skip test for format:%s (%dx%d)",
             CameraUtils::pixelCode2String(param.mFormat), param.mWidth, param.mHeight);
        camera_hal_deinit();
        return;
    }

    ret = camera_device_open(param.mCameraId);
    EXPECT_EQ(ret, 0);

    stream_t stream = camera_device_config_stream_normal(param.mCameraId, config,
                                                         V4L2_MEMORY_USERPTR);
    BufferLoopThread bufferLoop(param.mCameraId, stream);

    ret = bufferLoop.allocateBuffers();
    EXPECT_EQ(ret, OK);

    bufferLoop.queueAllBuffers();

    ret = camera_device_start(param.mCameraId);
    EXPECT_EQ(ret, 0);

    bufferLoop.run("buffer_loop", PRIORITY_FOREGROUND);

    // Let the buffer be looping for a while.
    std::this_thread::sleep_for(std::chrono::seconds(2));

    if (param.mStopBufferFirst) {
        bufferLoop.requestExitAndWait();

        ret = camera_device_stop(param.mCameraId);
        EXPECT_EQ(ret, 0);
        camera_device_close(param.mCameraId);
    } else {
        ret = camera_device_stop(param.mCameraId);
        EXPECT_EQ(ret, 0);

        // Randomly check what happens when close device at different time slot.
        bool closeFirst = rand() % 10 < 5;
        if (closeFirst) {
            camera_device_close(param.mCameraId);
        }

        // Try to dequeue buffer even after device stopped.
        std::this_thread::sleep_for(std::chrono::seconds(1));
        bufferLoop.requestExitAndWait();

        if (!closeFirst) {
            camera_device_close(param.mCameraId);
        }
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, multi_thread_stop_buffer_loop_first)
{
    int cameraId = getCurrentCameraId();

    TestParam param {cameraId, 1920, 1080, V4L2_PIX_FMT_NV12, true};
    mutil_thread_buffer_loop_common(param);
}

TEST_F(camHalTest, multi_thread_stop_device_first)
{
    int cameraId = getCurrentCameraId();

    TestParam param {cameraId, 1920, 1080, V4L2_PIX_FMT_NV12, false};
    mutil_thread_buffer_loop_common(param);
}

