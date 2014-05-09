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

#pragma once

#include <set>
#include <list>

#include "iutils/Thread.h"

#include "V4l2Dev.h"

#include "BufferQueue.h"
#include "CameraBuffer.h"
#include "IspParamAdaptor.h"

namespace icamera {

class DeviceCallback {
public:
    DeviceCallback() {};
    virtual ~DeviceCallback() {};
    virtual void onDequeueBuffer() {};
};

/**
 * DeviceBase is a base class of other devices which are for a particular purpose.
 * It provides: general operation of V4l2 devices, and secured buffer management.
 *
 * There are several virtual functions for subclass to override. The subclass should
 * base on what its implementation is to override one or several of them.
 */
class DeviceBase : public EventSource {
public:
    DeviceBase(int cameraId, VideoNodeType nodeType,
               VideoNodeDirection nodeDirection, DeviceCallback* deviceCB = nullptr);
    virtual ~DeviceBase();

    int configure(Port port, const stream_t& config);

    int openDevice();
    void closeDevice();

    int streamOn();
    int streamOff();

    int queueBuffer(long sequence);
    int dequeueBuffer();

    void addFrameListener(BufferConsumer *listener) { mConsumers.insert(listener); }
    void removeFrameListener(BufferConsumer *listener) { mConsumers.erase(listener); }
    void removeAllFrameListeners() { mConsumers.clear(); }

    bool hasPendingBuffer();
    void addPendingBuffer(const shared_ptr<CameraBuffer>& buffer);
    long getPredictSequence();
    int getBufferNumInDevice();
    void resetBuffers();
    bool skipFrameAfterSyncCheck(long sequence);

    V4l2Dev* getV4l2Device() { return mDevice; }
    const char* getName() { return mName; }
    Port getPort() { return mPort; }

protected:
    /**
     * Configure the device and request or create(if needed) the buffer pool.
     */
    virtual int createBufferPool(const stream_t& config) { return OK; }

    /**
     * Pre-process the buffer which to be queued to the device.
     */
    virtual int onQueueBuffer(long sequence, shared_ptr<CameraBuffer> buffer) { return OK; }

    /**
     * Post-process the buffer after it's dequeued from the device.
     */
    virtual int onDequeueBuffer(shared_ptr<CameraBuffer> buffer) { return OK; }

    /**
     * Return whether the buffer needs to be queued back to mPendingBuffers.
     */
    virtual bool needQueueBack(shared_ptr<CameraBuffer> buffer) { return false; }

    void dumpFrame(const shared_ptr<CameraBuffer>& buffer);

private:
    DISALLOW_COPY_AND_ASSIGN(DeviceBase);

    /**
     * Get one available buffer from mBuffersInDevice
     *
     * Return the front buffer of mBuffersInDevice if available, otherwise return nullptr.
     */
    shared_ptr<CameraBuffer> getFirstDeviceBuffer();

    /**
     * Pop the first buffer in mBuffersInDevice.
     * Add the buffer back to mPendingBuffers if needed.
     */
    void popBufferFromDevice();

protected:
    int mCameraId;
    Port mPort;
    VideoNodeType mNodeType;
    VideoNodeDirection mNodeDirection;
    const char* mName;
    V4l2Dev* mDevice; // The device used to queue/dequeue buffers.
    long mLatestSequence; // Track the latest bufffer sequence from driver.
    bool mNeedSkipFrame; // True if the frame/buffer needs to be skipped.
    int mFrameSkipNum; // How many frames need to be skipped after stream on.
    DeviceCallback* mDeviceCB;
    set<BufferConsumer*> mConsumers;

    /**
     * Each device has below three structures to manager its buffers.
     * And please note that:
     * 1. If the buffer is not allocated inside CaptureUnit, mAllocatedBuffers will be empty.
     * 2. Buffer to be queued into drive comes from mPendingBuffers.
     * 3. Buffer to be dequeued from driver comes from mBuffersInDevice.
     * 4. To make code clean, no null CameraBuffer is allowed to be put into these structures.
     * 5. The buffer cannot be in both mPendingBuffers and mBuffersInDevice.
     *    We must make the data consistent.
     */
    vector<shared_ptr<CameraBuffer>> mAllocatedBuffers; // Save all buffers allocated internally.
    list<shared_ptr<CameraBuffer>> mPendingBuffers;  // The buffers that are going to be queued.
    list<shared_ptr<CameraBuffer>> mBuffersInDevice; // The buffers that have been queued.
    Mutex mBufferLock; // The lock for protecting the internal buffers.
};

/**
 * MainDevice is a most commonly used device.
 * It's usually for producing video frames.
 */
class MainDevice : public DeviceBase {
public:
    MainDevice(int cameraId, VideoNodeType nodeType, DeviceCallback* deviceCB);
    ~MainDevice();

private:
    int createBufferPool(const stream_t& config);
    int onDequeueBuffer(shared_ptr<CameraBuffer> buffer);
    bool needQueueBack(shared_ptr<CameraBuffer> buffer);
};

/**
 * DolCaptureDevice is used for producing DOL HDR frames.
 */
class DolCaptureDevice : public DeviceBase {
public:
    DolCaptureDevice(int cameraId, VideoNodeType nodeType);
    ~DolCaptureDevice();

private:
    int createBufferPool(const stream_t& config);
    int onDequeueBuffer(shared_ptr<CameraBuffer> buffer);
    bool needQueueBack(shared_ptr<CameraBuffer> buffer);
};

/**
 * IsaRawDevice is used for producing ISA raw frames.
 * IsaRawDevice is usually used with MainDevice together.
 */
class IsaRawDevice : public DeviceBase {
public:
    IsaRawDevice(int cameraId, VideoNodeType nodeType);
    ~IsaRawDevice();

private:
    int createBufferPool(const stream_t& config);
    int onDequeueBuffer(shared_ptr<CameraBuffer> buffer);
    bool needQueueBack(shared_ptr<CameraBuffer> buffer);
};

/**
 * IsaConfigDevice is used to configure ISA with encoded parameters.
 */
class IsaConfigDevice : public DeviceBase {
public:
    IsaConfigDevice(int cameraId, VideoNodeType nodeType, IspParamAdaptor* isaAdaptor);
    ~IsaConfigDevice();

private:
    int createBufferPool(const stream_t& config);
    int onQueueBuffer(long sequence, shared_ptr<CameraBuffer> buffer);
    bool needQueueBack(shared_ptr<CameraBuffer> buffer) { return true; }

private:
    IspParamAdaptor* mIsaAdaptor;
};

/**
 * IsaStatsDevice is used for producing ISYS statistics.
 */
class IsaStatsDevice : public DeviceBase {
public:
    IsaStatsDevice(int cameraId, VideoNodeType nodeType, IspParamAdaptor* isaAdaptor);
    ~IsaStatsDevice();

private:
    int createBufferPool(const stream_t& config);
    int onQueueBuffer(long sequence, shared_ptr<CameraBuffer> buffer);
    int onDequeueBuffer(shared_ptr<CameraBuffer> buffer);

private:
    IspParamAdaptor* mIsaAdaptor;
};

} // namespace icamera

