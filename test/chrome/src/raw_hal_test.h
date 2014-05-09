/*
 * Copyright (C) 2016-2018 Intel Corporation
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
#ifndef RAW_HAL_TEST_H
#define RAW_HAL_TEST_H

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <chrono>
#include <gtest/gtest.h>
#include <hardware/camera3.h>
#include <camera/camera_metadata.h>

#ifndef GBM_BUFFER
#include "ui/GraphicBuffer.h"
#include "ui/GraphicBufferMapper.h"
#endif

#include "test_parameterization.h"
#include "test_utils.h"
#include "test_stream_factory.h"
#include "camera_metadata_hidden.h"

#ifdef USE_LOCAL_BUFFER
#include <stdlib.h>  // posix_memalign
#include <stddef.h>  //size_t
#include <string.h>
#include <errno.h>  // errno
#include <malloc.h> // malloc
#endif

#ifdef GBM_BUFFER
#include "camera3_test_gralloc.h"
using namespace camera3_test;
#endif

using namespace android;
using std::vector;
namespace Paramz = Parameterization;

/**
 * This header is a value-parmeterized "abstract test" definition.
 *
 * Including this header file allows reusing the test fixture class
 * in multiple test setups. Each including file will specify their own test cases.
 *
 * For more information, see:
 * https://github.com/google/googletest/blob/master/googletest/docs/AdvancedGuide.md
 */

extern camera_module_t *HAL_MODULE_INFO_SYM_PTR;

const uint64_t NS_ONE_SECOND = 1000000000;
const uint32_t MAX_VIDEO_RESOLUTION = 3840 * 2160;

extern bool gValgrindRun;
extern bool gDumpEveryFrame;

#define VALGRIND_MULTIPLIER (gValgrindRun ? 10 : 1)

//frame counts for multi streams test
const uint32_t FRAMES_FOR_MULTI_STREAMS = 20;

class condition_ : public std::condition_variable {
public:
    condition_() {};

    int waitRelative(std::unique_lock<std::mutex> &lock, uint64_t time_nsecs) {
      int ret = 0;
      std::chrono::nanoseconds nsec(time_nsecs);

      PRINTLN("Condition wait: %ld", time_nsecs);
      if (wait_for(lock, nsec) == std::cv_status::timeout) {
        ret = 1;
      }

      return ret;
    };
};

 /**
 * Tests for HAL Capture functionality
 */
class Basic_Test : public ::testing::Test, private camera3_callback_ops
{
public:

    int32_t mCameraId;
    hw_device_t *mDevice;
    uint8_t mProgress;

    std::mutex mTestMutex;
    condition_ mTestCondition;
    int mJpegFrameNumber; /**< hack to avoid counting returned request buffers */
    int mFramesCompleted;
    int mMetaResultCompleted;
    int mRequestsIssued;
    int mDumpAfterFrame; /**< dump image buffer when result frame number is equal or greater than this */
    bool m3AConverged;
    int64_t mConvergedExposureTime;
    int mConvergedIso;
    vector<camera3_stream_buffer> mBuffers;
    vector<camera3_stream_buffer> mPreviewBuffers;
    vector<camera3_stream_buffer> mYuvBuffers;
    vector<camera3_stream_buffer> mJpegBuffers;
#ifdef USE_LOCAL_BUFFER
    vector<void*> mAllocatedBuffers;
#else
#ifdef GBM_BUFFER
    vector<BufferHandleUniquePtr> mAllocatedBuffers;
#else
    vector<sp<GraphicBuffer>> mAllocatedBuffers;
#endif
#endif
    const char *mTestCaseName;
    const char *mTestName;
    int mDumpCount;
    bool silencePrint;

    int mTestWidth;
    int mTestHeight;

    bool mTestCameraConfig;
    uint32_t mMetadataTag;
    uint8_t  mMetadataValue;
    int mTestStreams;

public:
    static void s_notify(const struct camera3_callback_ops *ops,
            const camera3_notify_msg_t *msg) {}
#ifdef GBM_BUFFER
    Camera3TestGralloc* mgralloc_;
#endif

    Basic_Test() :
        mCameraId(0),
        mDevice(NULL),
        mProgress(0),
        mJpegFrameNumber(0),
        mFramesCompleted(0),
        mMetaResultCompleted(0),
        mRequestsIssued(0),
        mDumpAfterFrame(0),
        m3AConverged(false),
        mConvergedExposureTime(0),
        mConvergedIso(0),
        mTestCaseName(NULL),
        mTestName(NULL),
        mDumpCount(0),
        silencePrint(false),
        mTestWidth(1920),
        mTestHeight(1080),
#ifdef GBM_BUFFER
        mTestCameraConfig(false),
        mgralloc_(Camera3TestGralloc::GetInstance()) {}
#else
        mTestCameraConfig(false) {}
#endif

    void dumpFile(const camera3_capture_result_t *result)
    {
        const camera3_stream_buffer_t &c3Buf = result->output_buffers[0];
        int width = c3Buf.stream->width;
        int height = c3Buf.stream->height;
        buffer_handle_t *pHandle = c3Buf.buffer;

        mDumpCount++;
        char filename[256];
        char format[20];
        switch (result->output_buffers[0].stream->format) {
            case HAL_PIXEL_FORMAT_YCbCr_420_888:
                snprintf(format, sizeof(format), "%s", "yuv420");
            break;
            case HAL_PIXEL_FORMAT_BLOB:
                snprintf(format, sizeof(format), "%s", "jpeg");
            break;
            case HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED:
            default:
                snprintf(format, sizeof(format), "%s", "nv12");
        }
        snprintf(filename,
                 sizeof(filename),
                 "CameraId_%d_%s_%s_%d_%dx%d.%s",
                 mCameraId,
                 mTestCaseName,
                 mTestName,
                 mDumpCount,
                 width,
                 height,
                 format);

        std::string sFileName(filename);
        std::replace(sFileName.begin(), sFileName.end(), '/', '_');

        FILE *f = fopen(sFileName.c_str(), "w+");
        if (f == NULL) {
            PRINTLN("dump file opening failed, filename was %s", sFileName.c_str());
            return;
        }
        void *address;
#ifdef USE_LOCAL_BUFFER
        address = (void*) c3Buf.buffer;
#else
#ifndef GBM_BUFFER
        GraphicBufferMapper &gbm = GraphicBufferMapper::get();

        Rect bounds(width, height);
        status_t status = gbm.lock(static_cast<const native_handle*>(*pHandle),0,bounds,&address);
#else
        struct android_ycbcr out_ycbcr;

        if(result->output_buffers[0].stream->format == HAL_PIXEL_FORMAT_BLOB) {
            width = width * height;
            height = 1;
        }

        if (mgralloc_->GetFormat(*pHandle) == HAL_PIXEL_FORMAT_BLOB) {
           mgralloc_->Lock(*pHandle, 0, 0, 0, width, height, &address);
        } else {
            mgralloc_->LockYCbCr(*pHandle, 0, 0, 0, width, height, &out_ycbcr);
            address = out_ycbcr.y;
        }
#endif
#endif
        size_t size;
        if (result->output_buffers[0].stream->format == HAL_PIXEL_FORMAT_BLOB) {
            // for blobs the jpegs are allocated 1 byte per pixel
            // and during allocation the width is having the size,
            // height being 1. Meaning the allocated size is really
            // stream width * height * 1. This is also what the HAL
            // expects internally.

            // blob stream just allocates size width*height
            int bufferSize = width * height;
            // now the jpeg size is in camera3_jpeg_blob located at
            // byte[bufferSize - sizeof(camera3_jpeg_blob)]
            camera3_jpeg_blob *blob = (camera3_jpeg_blob *) &((char *)address)[bufferSize - sizeof(camera3_jpeg_blob)];
            size = blob->jpeg_size;
            PRINTLN("Jpeg received, ptr %x allocated buffer size is %d, jpeg id is %x and jpeg size %d",
                    address,
                    bufferSize,
                    blob->jpeg_blob_id,
                    size);

        } else {
            size = width * height * 3 / 2;
        }

        fwrite((char *)address, 1, size, f);
        fflush(f);
        fclose(f);
    }

    /* Keep locked by the caller */
    virtual void processCaptureResult(const camera3_capture_result_t *result)
    {
        PRINTLN("%s: captured: buffer %d, meta %p ", __func__, result->num_output_buffers, result->result);
        // we should handle case of num_output_buffers > 1 but that doesn't really happen so we don't care..
        if (result->num_output_buffers == 1) {
            if (mJpegFrameNumber == -1 || (result->output_buffers[0].stream->format == HAL_PIXEL_FORMAT_BLOB)) {
                PRINTLN("Received buffer: stream %p, 0x%x, frame_number %u", result->output_buffers[0].stream,
                        result->output_buffers[0].stream->format, result->frame_number);
                mFramesCompleted++;
                if (!silencePrint)
                    PRINTLN("c:%d frames completed: %d, metadata %d", mCameraId, mFramesCompleted, mMetaResultCompleted);

                if (result->frame_number >= mDumpAfterFrame) {
                    dumpFile(result);
                }

                if (result->output_buffers[0].stream->format == HAL_PIXEL_FORMAT_BLOB) {
                    mJpegBuffers.push_back(result->output_buffers[0]);
                } else {
                    mBuffers.push_back(result->output_buffers[0]);
                }

                if (mFramesCompleted % mTestStreams  == 0) {
                    PRINTLN("Notify all buffers completed.");
                    mTestCondition.notify_all();
                } else {
                    PRINTLN("Not yet all buffers completed.");
                }
            }
        }
        if (result->result) {
            // metadata, check 3A convergence
            CameraMetadata meta;
            meta = result->result; // clone (because const)
            mMetaResultCompleted++;
            if (!silencePrint)
                PRINTLN("c:%d frames completed: %d, metadata %d ", mCameraId, mFramesCompleted, mMetaResultCompleted);

            camera_metadata_entry aeState = meta.find(ANDROID_CONTROL_AE_STATE);
            camera_metadata_entry awbState = meta.find(ANDROID_CONTROL_AWB_STATE);
            camera_metadata_entry afState = meta.find(ANDROID_CONTROL_AF_STATE);
            camera_metadata_entry afMode = meta.find(ANDROID_CONTROL_AF_MODE);
            camera_metadata_entry aeMode = meta.find(ANDROID_CONTROL_AE_MODE);
            camera_metadata_entry awbMode = meta.find(ANDROID_CONTROL_AWB_MODE);

            if (aeState.count == 1 && awbState.count == 1 && afState.count == 1 &&
                aeMode.count == 1 && awbMode.count == 1 && afMode.count == 1) {
                if (!silencePrint)
                    PRINTLN("c:%d 3A frame %d state AE:%d AWB:%d AF:%d, mode AE:%d AWB:%d AF:%d",
                        mCameraId,
                        result->frame_number,
                        aeState.data.u8[0],
                        awbState.data.u8[0],
                        afState.data.u8[0],
                        aeMode.data.u8[0],
                        awbMode.data.u8[0],
                        afMode.data.u8[0]);

                bool afDone = (afState.data.u8[0] == ANDROID_CONTROL_AF_STATE_FOCUSED_LOCKED ||
                               afState.data.u8[0] == ANDROID_CONTROL_AF_STATE_PASSIVE_FOCUSED) ||
                               (afState.data.u8[0] == ANDROID_CONTROL_AF_STATE_INACTIVE &&
                                afMode.data.u8[0] == ANDROID_CONTROL_AF_MODE_OFF);

                if (!m3AConverged &&
                    (aeState.data.u8[0] == ANDROID_CONTROL_AE_STATE_CONVERGED ||
                     (aeState.data.u8[0] == ANDROID_CONTROL_AE_STATE_INACTIVE &&
                      aeMode.data.u8[0]  == ANDROID_CONTROL_AE_MODE_OFF)) &&
                    (awbState.data.u8[0] == ANDROID_CONTROL_AWB_STATE_CONVERGED ||
                     (awbState.data.u8[0] == ANDROID_CONTROL_AWB_STATE_INACTIVE &&
                      awbMode.data.u8[0] == ANDROID_CONTROL_AWB_MODE_OFF)) &&
                    afDone) {
                    m3AConverged = true;
                    // get exposure time and sensitivity
                    camera_metadata_entry exposureTimeEntry = meta.find(ANDROID_SENSOR_EXPOSURE_TIME);
                    camera_metadata_entry sensitivityEntry = meta.find(ANDROID_SENSOR_SENSITIVITY);
                    if (exposureTimeEntry.count == 1 && sensitivityEntry.count == 1) {
                        mConvergedExposureTime = exposureTimeEntry.data.i64[0];
                        mConvergedIso = sensitivityEntry.data.i32[0];
                    } else {
                        PRINTLN("metadata missing either exposure time or sensitivity");
                        mConvergedExposureTime = 0;
                        mConvergedIso = 0;
                    }
                }
            } else {
                PRINTLN("Cannot evaluate 3A convergence status. "
                        "Metadata entry counts are ae:%d awb:%d af:%d",
                        aeState.count, awbState.count, afState.count);
            }

            // Notify if the result has metadata only
            if (!result->num_output_buffers) {
                PRINTLN("Notify metadata result completed.");
                mTestCondition.notify_all();
            }
        }
    }

    static void s_process_capture_result(const struct camera3_callback_ops *ops,
            const camera3_capture_result_t *result)
    {
        Basic_Test *test =
                    const_cast<Basic_Test*>(static_cast<const Basic_Test*>(ops));

        test->processCaptureResult(result);
    }

    void SetUp()
    {
        std::lock_guard<std::mutex> lock(mTestMutex); // silence certain code analyzers
        mProgress = 0;
        mConvergedExposureTime = 0;
        mJpegFrameNumber = -1;
        mConvergedIso = 0;
        mRequestsIssued = 0;
        mDumpAfterFrame = INT_MAX;
        m3AConverged = false;
        mFramesCompleted = 0;
        mMetaResultCompleted = 0;
        mDevice = NULL;
        mTestStreams = 1;
        // clearing strictly not needed, but just making double-sure
        mBuffers.clear();
        mAllocatedBuffers.clear();
        uint32_t status = OK;
        mDumpCount = 0;

        const testing::TestInfo* const test_info =
            testing::UnitTest::GetInstance()->current_test_info();
        mTestName = test_info->name();
        mTestCaseName = test_info->test_case_name();

        char camera[20];
        sprintf(camera, "%d", mCameraId);

        int ret = HAL_MODULE_INFO_SYM_PTR->common.methods->
            open((hw_module_t *)HAL_MODULE_INFO_SYM_PTR, camera, &mDevice);
        ASSERT_EQ(ret, 0);

        ASSERT_NE(mDevice, nullptr) << "HAL Creation Failed";
        mProgress++;

        // set callbacks
        camera3_callback_ops::notify = &s_notify;
        camera3_callback_ops::process_capture_result = &s_process_capture_result;
        status = DOPS(mDevice)->initialize(CDEV(mDevice), this);
        ASSERT_EQ(status, OK) << "HAL initialize failed";
        mProgress++;

        if (HAL_MODULE_INFO_SYM_PTR->get_vendor_tag_ops != NULL) {
            static vendor_tag_ops_t ops;
            HAL_MODULE_INFO_SYM_PTR->get_vendor_tag_ops(&ops);
            //Note: Only availiable for framework internal use. from
            //       libcamera_metadata/include/camera_metadata_hidden.h
            set_camera_metadata_vendor_ops(&ops);
        }
    }

    void is4KSupported(bool &isSupported)
    {
        isSupported = false;
        // get the static metadata, which has available stream configs
        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
        const camera_metadata_t *meta = ac2info.static_camera_characteristics;

        ASSERT_NE(meta, nullptr);

        camera_metadata_ro_entry_t entry;
        entry.count = 0;
        int ret = find_camera_metadata_ro_entry(meta,
                                 ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                                 &entry);

        ASSERT_EQ(ret, OK);

        int count = entry.count;
        const int32_t *availStreamConfig = entry.data.i32;

        ASSERT_NE(availStreamConfig, nullptr);
        ASSERT_NE(true, (count < 4));

        // figure out if 4K is supported
        for (uint32_t j = 0; j < (uint32_t)count; j += 4) {
            // only process the nv12 outputs, for now
            if (availStreamConfig[j] == HAL_PIXEL_FORMAT_YCbCr_420_888 &&
                availStreamConfig[j+3] == CAMERA3_STREAM_OUTPUT) {
                int pixCount = availStreamConfig[j + 1] * availStreamConfig[j + 2];
                if (pixCount == 3840 * 2160) {
                    isSupported = true;
                    break;
                }
            }
        }
    }

    /**
     * \param [IN]  fps value
     * \param [OUT] outputMetadata where the fps target range is written
     */
    void setFps(int fps, CameraMetadata &outputMetadata)
    {
        // get the static metadata, which has available fps ranges
        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(mCameraId, &ac2info);
        const camera_metadata_t *staticMetadata = ac2info.static_camera_characteristics;

        if (staticMetadata == nullptr) {
            PRINTLN("Can't get static metadata");
            return;
        }

        camera_metadata_ro_entry fpsRangesEntry;
        fpsRangesEntry.count = 0;
        int ret = find_camera_metadata_ro_entry(staticMetadata,
                                                ANDROID_CONTROL_AE_AVAILABLE_TARGET_FPS_RANGES,
                                                &fpsRangesEntry);

        if (ret != OK) {
            PRINTLN("Getting FPS ranges failed");
            return;
        }
        if ((fpsRangesEntry.count < 2) || (fpsRangesEntry.count % 2)) {
             PRINTLN("No valid FPS ranges in static metadata");
             return;
        }

        // supported fps ranges are in (min, max) pairs
        // find the suitable range, prioritize fixed range
        int32_t lowFps = 0;
        int32_t highFps = 0;
        int32_t index = -1;
        for (int i = 0; i < fpsRangesEntry.count / 2; i++) {
            lowFps = fpsRangesEntry.data.i32[2 * i];
            highFps = fpsRangesEntry.data.i32[2 * i + 1];

            // found suitable fixed range, stop searching
            if (fps == lowFps && fps == highFps) {
                index = 2*i;
                break;
            }

            // suitable variable range found
            if (fps >= lowFps && fps <= highFps) {
                index = 2*i;
            }

        }

        if (index == -1) {
            PRINTLN("Suitable range not found for fps setting %d, using default", fps);
            index = 0;
        }

        // set target range
        int32_t fpsRange[2];
        fpsRange[0] = fpsRangesEntry.data.i32[index];
        fpsRange[1] = fpsRangesEntry.data.i32[index + 1];
        PRINTLN("Setting target fps range [%d, %d]", fpsRange[0], fpsRange[1]);

        outputMetadata.update(ANDROID_CONTROL_AE_TARGET_FPS_RANGE,
                              fpsRange,
                              2);
    }

    /*
     * returns a camera_metadata_t * with settings. Ownership transfers to
     * the caller and user of this API must release the buffer
     */
    camera_metadata_t * constructRequestSettings(camera3_request_template_t reqTemplate)
    {
        const camera_metadata_t *requestSettings = DOPS(mDevice)->
                construct_default_request_settings(CDEV(mDevice), reqTemplate);

        CameraMetadata metadata;
        metadata = requestSettings; // copy settings from HAL
        // add the request id
        int32_t requestId = 0;
        metadata.update(ANDROID_REQUEST_ID, &requestId, 1);

        return metadata.release();
    }

    template<typename T> bool hasMetadataValue(uint32_t tag, T value, const camera_metadata_t *meta)
    {
        if (meta == NULL)
            return false;

        camera_metadata_ro_entry_t entry;
        entry.count = 0;
        int ret = find_camera_metadata_ro_entry(meta, tag, &entry);
        if (ret != OK)
            return false;

        const T *data = (const T *) entry.data.i64;

        for (size_t i = 0; i < entry.count; i++) {
            if (data[i] == value)
                return true;
        }

        return false;
    }

    bool isManualFocusSupported(int cameraId)
    {
        // get the static metadata
        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(cameraId, &ac2info);
        const camera_metadata_t *meta = ac2info.static_camera_characteristics;

        bool hasOff = hasMetadataValue<uint8_t>(ANDROID_CONTROL_AF_AVAILABLE_MODES, ANDROID_CONTROL_AF_MODE_OFF, meta);
        bool hasDistance = !hasMetadataValue<float>(ANDROID_LENS_INFO_MINIMUM_FOCUS_DISTANCE, 0.0f, meta);
        bool hasSensorControl = hasMetadataValue<uint8_t>(ANDROID_REQUEST_AVAILABLE_CAPABILITIES, ANDROID_REQUEST_AVAILABLE_CAPABILITIES_MANUAL_SENSOR, meta);

        return (hasOff && hasDistance && hasSensorControl);
    }

    void setManualFocus(CameraMetadata &outputMetadata, float distance)
    {
        uint8_t afMode = ANDROID_CONTROL_AF_MODE_OFF;
        outputMetadata.update(ANDROID_CONTROL_AF_MODE, &afMode, 1);
        // a check for distance being within allowed limits could be added here
        float focusDistance = distance;
        outputMetadata.update(ANDROID_LENS_FOCUS_DISTANCE, &focusDistance, 1);
    }

    status_t allocateBuffers(int bufferCount,
                             camera3_stream_t *stream,
                             vector<camera3_stream_buffer> *buffers = NULL)
    {
        status_t status = OK;
        if (buffers == NULL)
            buffers = &mBuffers;

        for (int i = 0 ; i < bufferCount; i++) {
            camera3_stream_buffer streamBuffer;
            // for jpeg, the buffer pixel count must be put into width field
            // for allocation, HAL expects this.
            int width = stream->width;
            int height = stream->height;

            if (stream->format == HAL_PIXEL_FORMAT_BLOB) {
                width = stream->width * stream->height;
                height = 1;
            }

#ifdef USE_LOCAL_BUFFER
            void *address;
            int mem_size = width*height*2;
            int page_size = getpagesize();
            status = posix_memalign(&address, page_size, mem_size);
            if(status != 0) {
                PRINTLN("posix_memalign failed : %s", strerror(status));
                return NO_MEMORY;
            }
            streamBuffer.stream = stream;
            streamBuffer.acquire_fence = -1;
            streamBuffer.release_fence = -1;
            streamBuffer.buffer = (buffer_handle_t *)address;
            streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
            PRINTLN("@TestMultiBuffers%d width-height:%d-%d, allocated buffer handle : %p posix_memalign : %p for stream %p", \
                                        i, \
                                        width, height,  \
                                        streamBuffer.buffer,  \
                                        address,  \
                                        stream);
            buffers->push_back(streamBuffer);
            mAllocatedBuffers.push_back(address);
#else
#ifndef GBM_BUFFER
            GraphicBuffer *gb = new GraphicBuffer(width,
                                                  height,
                                                  stream->format,
                                                  stream->usage);
            if (gb == NULL)
                return NO_MEMORY;

            status = gb->initCheck();
            if (status != OK)
                return status;

            streamBuffer.stream = stream;
            streamBuffer.acquire_fence = 0;
            streamBuffer.release_fence = 0;
            streamBuffer.buffer = &gb->getNativeBuffer()->handle;

            buffers->push_back(streamBuffer);
            sp<GraphicBuffer> spBuf = gb;
            mAllocatedBuffers.push_back(spBuf);
#else
            PRINTLN("Call Allocate: mgralloc_ %p, format 0x%x", mgralloc_, stream->format);
            BufferHandleUniquePtr bufh = mgralloc_->Allocate(width,
                                                             height,
                                                             stream->format,
                                                             GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE); // usage

            if (bufh == nullptr)
                return NO_MEMORY;
            streamBuffer.stream = stream;
            streamBuffer.acquire_fence = -1;
            streamBuffer.release_fence = -1;
            streamBuffer.buffer = bufh.get();
            streamBuffer.status = CAMERA3_BUFFER_STATUS_OK;
            PRINTLN("Allocated buffer: %p for stream %p", streamBuffer.buffer, stream);
            buffers->push_back(streamBuffer);
            mAllocatedBuffers.push_back(std::move(bufh));
#endif
#endif
        }
        return status;
    }

    status_t allocateBuffers(int32_t width,
                             int32_t height,
                             int32_t format,
                             int32_t usage,
                             camera3_stream *stream,
                             camera3_stream_buffer *streamBuffer)
    {

        status_t status = OK;
        if (stream == NULL || streamBuffer == NULL)
            return BAD_VALUE;

#ifdef USE_LOCAL_BUFFER
        void *address;
        int mem_size = width*height*2;
        int page_size = getpagesize();
        status = posix_memalign(&address, page_size, mem_size);
        if(status != 0) {
            PRINTLN("posix_memalign failed : %s", strerror(status));
            return NO_MEMORY;
        }
        streamBuffer->stream = stream;
        streamBuffer->acquire_fence = -1;
        streamBuffer->release_fence = -1;
        streamBuffer->buffer = (buffer_handle_t *)address;
        streamBuffer->status = CAMERA3_BUFFER_STATUS_OK;
        PRINTLN("@TestOneBuffer width-height:%d-%d, allocated buffer handle : %p posix_memalign : %p for stream %p", \
                                    width, height,  \
                                    streamBuffer->buffer,  \
                                    address,  \
                                    stream);
        mAllocatedBuffers.push_back(address);
#else
#ifndef GBM_BUFFER
        GraphicBuffer *gb = new GraphicBuffer(width,
                                              height,
                                              format,
                                              usage);
        //ASSERT_TRUE(gb != NULL);// << "Failed to instantiate Graphic Buffer";

        sp<GraphicBuffer> spGB = gb; // for clean up

        status = gb->initCheck();
        //ASSERT_EQ(status, 0);// << "Graphic Buffer initialization failed: status" \
                             //<< std::hex <<  status;
        streamBuffer->stream = stream;
        streamBuffer->acquire_fence = 0;
        streamBuffer->release_fence = 0;
        streamBuffer->buffer = &gb->getNativeBuffer()->handle;
#else
        PRINTLN("Call Allocate: mgralloc_ %p, format 0x%x, stream format 0x%x", mgralloc_, format, stream->format);
        BufferHandleUniquePtr bufh = mgralloc_->Allocate(width,
                                                         height,
                                                         stream->format,
                                                         GRALLOC_USAGE_SW_WRITE_OFTEN | GRALLOC_USAGE_HW_CAMERA_WRITE); // usage
        if (bufh == nullptr)
            return NO_MEMORY;
        streamBuffer->stream = stream;
        streamBuffer->acquire_fence = -1;
        streamBuffer->release_fence = -1;
        streamBuffer->buffer = bufh.get();
        PRINTLN("Allocated buffer: %p for stream %p", streamBuffer->buffer, stream);
        streamBuffer->status = CAMERA3_BUFFER_STATUS_OK;
        mAllocatedBuffers.push_back(std::move(bufh));
#endif
#endif
        return status;
    }

    status_t createSingleStreamConfig(camera3_stream_configuration_t &streamConfig,
                                      camera3_stream_t &stream,
                                      camera3_stream_t *streamPtrs[1],
                                      int width,
                                      int height)
    {
        streamPtrs[0] = &stream;

        streamConfig.num_streams = 1;
        streamConfig.operation_mode = 0;
        streamConfig.streams = streamPtrs;

        stream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
        stream.width = width;
        stream.height = height;
        stream.stream_type = CAMERA3_STREAM_OUTPUT;
        stream.usage = (width * height > MAX_VIDEO_RESOLUTION) ? 0 :
                       GRALLOC_USAGE_HW_COMPOSER; // to force video
        stream.priv = NULL;
        stream.max_buffers = 2;
        stream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        status_t status = DOPS(mDevice)->configure_streams(CDEV(mDevice), &streamConfig);
        return status;
    }


    status_t create3StreamsConfig(camera3_stream_configuration_t &streamConfig,
                                      camera3_stream_t &_1stStream,
                                      camera3_stream_t &_2ndStream,
                                      camera3_stream_t &_3rdStream,
                                      camera3_stream_t *streamPtrs[3],
                                      Parmz::MultiStreamsTestParam params)
    {
        int _1stWidth = params.params[0].width;
        int _1stHeight = params.params[0].height;
        int _1stFormat = params.params[0].format;
        int _2ndWidth = params.params[1].width;
        int _2ndHeight = params.params[1].height;
        int _2ndFormat = params.params[1].format;
        int _3rdWidth = params.params[2].width;
        int _3rdHeight = params.params[2].height;
        int _3rdFormat = params.params[2].format;

        streamPtrs[0] = &_1stStream;
        streamPtrs[1] = &_2ndStream;
        streamPtrs[2] = &_3rdStream;

        streamConfig.num_streams = 3;
        streamConfig.operation_mode = 0;
        streamConfig.streams = streamPtrs;

        _1stStream.format = _1stFormat;
        _1stStream.width = _1stWidth;
        _1stStream.height = _1stHeight;
        _1stStream.stream_type = CAMERA3_STREAM_OUTPUT;
        _1stStream.usage = GRALLOC_USAGE_HW_COMPOSER;
        _1stStream.priv = NULL;
        _1stStream.max_buffers = 1;
        _1stStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        _2ndStream.format = _2ndFormat;
        _2ndStream.width = _2ndWidth;
        _2ndStream.height = _2ndHeight;
        _2ndStream.stream_type = CAMERA3_STREAM_OUTPUT;
        _2ndStream.usage = 0;
        _2ndStream.priv = NULL;
        _2ndStream.max_buffers = 1;
        _2ndStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        _3rdStream.format = _3rdFormat;
        _3rdStream.width = _3rdWidth;
        _3rdStream.height = _3rdHeight;
        _3rdStream.stream_type = CAMERA3_STREAM_OUTPUT;
        _3rdStream.usage = 0;
        _3rdStream.priv = NULL;
        _3rdStream.max_buffers = 1;
        _3rdStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;


        status_t status = DOPS(mDevice)->configure_streams(CDEV(mDevice), &streamConfig);
        return status;
    }

    status_t create2StreamsConfig(camera3_stream_configuration_t &streamConfig,
                                      camera3_stream_t &_1stStream,
                                      camera3_stream_t &_2ndStream,
                                      camera3_stream_t *streamPtrs[2],
                                      Paramz::MultiStreamsTestParam params)
    {
        int _1stWidth = params.params[0].width;
        int _1stHeight = params.params[0].height;
        int _1stFormat = params.params[0].format;
        int _2ndWidth = params.params[1].width;
        int _2ndHeight = params.params[1].height;
        int _2ndFormat = params.params[1].format;

        streamPtrs[0] = &_1stStream;
        streamPtrs[1] = &_2ndStream;

        streamConfig.num_streams = 2;
        streamConfig.operation_mode = 0;
        streamConfig.streams = streamPtrs;

        _1stStream.format = _1stFormat;
        _1stStream.width = _1stWidth;
        _1stStream.height = _1stHeight;
        _1stStream.stream_type = CAMERA3_STREAM_OUTPUT;
        _1stStream.usage = GRALLOC_USAGE_HW_COMPOSER;
        _1stStream.priv = NULL;
        _1stStream.max_buffers = 1;
        _1stStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        _2ndStream.format = _2ndFormat;
        _2ndStream.width = _2ndWidth;
        _2ndStream.height = _2ndHeight;
        _2ndStream.stream_type = CAMERA3_STREAM_OUTPUT;
        _2ndStream.usage = 0;
        _2ndStream.priv = NULL;
        _2ndStream.max_buffers = 1;
        _2ndStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        status_t status = DOPS(mDevice)->configure_streams(CDEV(mDevice), &streamConfig);
        return status;
    }

    void process2StreamsRequests(int frameCount, Paramz::MultiStreamsTestParam params)
    {
        uint32_t status = 0;
        camera3_stream_configuration_t streamConfig;
        camera3_stream_t streams[2];
        camera3_stream_t *streamPtrs[2];
        camera3_capture_request_t request;
        mTestStreams = 2;
        //wait max 5s when request single frame
        int waitTime = (frameCount <= 1) ? 5 : 1;

        status_t err = create2StreamsConfig(streamConfig, streams[0], streams[1],
                                            streamPtrs, params);
        ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                          << std::hex <<  status;

        camera_metadata_t *requestSettings =
                constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

        std::vector<camera3_stream_buffer> output_buffers;

        // Allocate memory for stream 0
        status = allocateBuffers(1, &streams[0], &output_buffers);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        // Allocate memory for stream 1
        status = allocateBuffers(1, &streams[1], &output_buffers);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        request.num_output_buffers = 2;
        request.input_buffer = NULL;
        request.settings = requestSettings;
        request.output_buffers = output_buffers.data();

        mDumpAfterFrame = gDumpEveryFrame ? 0 : (frameCount - 1);
        std::unique_lock<std::mutex> lock(mTestMutex);
        for (int i = 0; i < frameCount; i++) {
            request.frame_number = i;
            status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                                << std::hex <<  status;

            int ret = mTestCondition.waitRelative(lock, waitTime * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
            ASSERT_EQ(ret, 0) << "Request did not complete in " << waitTime << " seconds";
        }

        free_camera_metadata(requestSettings);
    }

    void process3StreamsBlobRequests(Parmz::MultiStreamsTestParam params)
    {
        uint32_t status = 0;
        camera3_stream_configuration_t streamConfig;
        camera3_stream_t streams[3];
        camera3_stream_t *streamPtrs[3];
        camera3_capture_request_t request;
        mTestStreams = 3;
        int jpegframeCount = 10;
        //wait max 5s when request single frame
        int waitTime = 5;

        status_t err = create3StreamsConfig(streamConfig, streams[0], streams[1],
                                            streams[2], streamPtrs, params);
        ASSERT_EQ(err, 0) << "HAL stream config failed status: " \
                          << std::hex <<  status;

        camera_metadata_t *requestSettings =
                constructRequestSettings(CAMERA3_TEMPLATE_PREVIEW);

        std::vector<camera3_stream_buffer> output_buffers;

        // Allocate memory for stream 0
        status = allocateBuffers(1, &streams[0], &output_buffers);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        // Allocate memory for stream 1
        status = allocateBuffers(1, &streams[1], &output_buffers);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        // Allocate memory for stream 2
        status = allocateBuffers(1, &streams[2], &output_buffers);
        ASSERT_EQ(status, OK) << "Buffer allocation failed";

        request.num_output_buffers = 3;
        request.input_buffer = NULL;
        request.settings = requestSettings;
        request.output_buffers = output_buffers.data();

        mDumpAfterFrame = gDumpEveryFrame ? 0 : (jpegframeCount - 1);
        std::unique_lock<std::mutex> lock(mTestMutex);
        for (int i = 0; i < jpegframeCount; i++) {
            request.frame_number = i;
            status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                                << std::hex <<  status;

            int ret = mTestCondition.waitRelative(lock, waitTime * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
            ASSERT_EQ(ret, 0) << "Request did not complete in " << waitTime << " seconds";
        }

        // wait that requests are finished
        //waitFramesToComplete(jpegframeCount * request.num_output_buffers);
        free_camera_metadata(requestSettings);

    }

    void createJpegStreamConfig(camera3_stream_configuration_t &streamConfig,
                                camera3_stream_t &previewStream,
                                camera3_stream_t &jpegStream,
                                camera3_stream_t *streamPtrs[],
                                int width,
                                int height,
                                int jpegWidth,
                                int jpegHeight)
    {
        streamPtrs[0] = &previewStream;
        streamPtrs[1] = &jpegStream;

        streamConfig.num_streams = 2;
        streamConfig.operation_mode = 0;
        streamConfig.streams = streamPtrs;

        previewStream.format = HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED;
        previewStream.width = width;
        previewStream.height = height;
        previewStream.stream_type = CAMERA3_STREAM_OUTPUT;
        previewStream.usage = GRALLOC_USAGE_HW_COMPOSER; // to force video
        previewStream.priv = NULL;
        previewStream.max_buffers = 2;
        previewStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        jpegStream.format = HAL_PIXEL_FORMAT_BLOB;
        jpegStream.width = jpegWidth;
        jpegStream.height = jpegHeight;
        jpegStream.stream_type = CAMERA3_STREAM_OUTPUT;
        jpegStream.usage = 0;
        jpegStream.priv = NULL;
        jpegStream.max_buffers = 2;
        jpegStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        status_t status = DOPS(mDevice)->configure_streams(CDEV(mDevice), &streamConfig);
        ASSERT_EQ(status, 0) << "HAL stream config failed status: " \
                             << std::hex <<  status;
    }

    status_t createSingleStreamJpegStreamConfig(camera3_stream_configuration_t &streamConfig,
                                                camera3_stream_t &jpegStream,
                                                camera3_stream_t *streamPtrs[],
                                                int jpegWidth,
                                                int jpegHeight)
    {
        streamPtrs[0] = &jpegStream;

        streamConfig.num_streams = 1;
        streamConfig.operation_mode = 0;
        streamConfig.streams = streamPtrs;

        jpegStream.format = HAL_PIXEL_FORMAT_BLOB;
        jpegStream.width = jpegWidth;
        jpegStream.height = jpegHeight;
        jpegStream.stream_type = CAMERA3_STREAM_OUTPUT;
        jpegStream.usage = 0;
        jpegStream.priv = NULL;
        jpegStream.max_buffers = 2;
        jpegStream.crop_rotate_scale_degrees = CAMERA3_STREAM_ROTATION_0;

        status_t status = DOPS(mDevice)->configure_streams(CDEV(mDevice), &streamConfig);
        return status;
    }

    void processMultiBufferRequests(int frameCount,
                                    camera3_capture_request_t &request,
                                    bool stopAt3AConvergence = false,
                                    float numSeconds = 1.0f)
    {
        camera_metadata_t *settings;
        const camera_metadata_t *origSettings = request.settings;
        std::unique_lock<std::mutex> lock(mTestMutex);

        if (mTestCameraConfig) {
            CameraMetadata meta;
            meta = request.settings;
            meta.update(mMetadataTag, &mMetadataValue, 1);
            settings = meta.release();
        }

        for (int i = 0; i < frameCount; i++) {
            // bail if 3A converged
            if (stopAt3AConvergence && m3AConverged) {
                return;
            }

            if (mBuffers.size() == 0) {
                mTestCondition.waitRelative(lock, numSeconds * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mBuffers.size() == 0) {
                    // TODO: asserts not good here in sub-functions
                    ASSERT_TRUE(false) << "timed out waiting for buffers";
                }
            }
            request.frame_number = i;
            camera3_stream_buffer streamBuffer = mBuffers[0];
            mBuffers.erase(mBuffers.begin());
            request.output_buffers = &streamBuffer;
            if (mTestCameraConfig) {
                request.settings = settings;
            }

            mRequestsIssued++; // a bit premature, but it will assert if it fails
            status_t status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                                 << std::hex <<  status;

        }

        if (mTestCameraConfig) {
            free_camera_metadata(settings);
            request.settings = origSettings;
        }
    }

    void processJpegRequests(int frameCount,
                             camera3_capture_request_t &request)
    {
        std::unique_lock<std::mutex> lock(mTestMutex);

        for (int i = 0; i < frameCount; i++) {
            if (mBuffers.size() == 0) {
                mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mBuffers.size() == 0) {
                    ASSERT_TRUE(false) << "timed out waiting for buffers";
                }
            }
            if (mJpegBuffers.size() == 0) {
                mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mJpegBuffers.size() == 0) {
                    ASSERT_TRUE(false) << "timed out waiting for jpeg buffers";
                }
            }

            request.frame_number = i;
            camera3_stream_buffer streamBuffers[2];
            streamBuffers[0] = mBuffers[0];
            streamBuffers[1] = mJpegBuffers[0];
            mBuffers.erase(mBuffers.begin());
            mJpegBuffers.erase(mJpegBuffers.begin());
            request.output_buffers = streamBuffers;

            mRequestsIssued++; // a bit premature, but it will assert if it fails
            status_t status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                                 << std::hex <<  status;
        }
    }

    status_t processSingleStreamJpegRequests(int frameCount,
                                             camera3_capture_request_t &request)
    {
        status_t status = UNKNOWN_ERROR;
        camera_metadata_t *setting;
        std::unique_lock<std::mutex> lock(mTestMutex);

        if (mTestCameraConfig) {
            CameraMetadata meta;
            meta = request.settings;
            meta.update(mMetadataTag, &mMetadataValue, 1);
            setting = meta.release();
        }

        for (int i = 0; i < frameCount; i++) {
            if (mJpegBuffers.size() == 0) {
                mTestCondition.waitRelative(lock, 2 * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mJpegBuffers.size() == 0) {
                    status = UNKNOWN_ERROR;
                    return status;
                }
            }

            request.frame_number = i;
            camera3_stream_buffer streamBuffers[1];
            streamBuffers[0] = mJpegBuffers[0];
            mJpegBuffers.erase(mJpegBuffers.begin());
            request.output_buffers = streamBuffers;
            if (mTestCameraConfig) {
                request.settings = setting;
            }

            mRequestsIssued++; // a bit premature, but it will assert if it fails
            status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            if (status != OK)
                break;
        }

        if (mTestCameraConfig) {
            free_camera_metadata(setting);
        }

        return status;
    }

    void waitMetaResultToComplete(int frameCount)
    {
        std::unique_lock<std::mutex> lock(mTestMutex);

        for (int i = 0; i < frameCount; i++) {
            if (mMetaResultCompleted < frameCount) {
                mTestCondition.waitRelative(lock, 2 * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mMetaResultCompleted == frameCount)
                    break;
            }
        }
        EXPECT_EQ(mMetaResultCompleted, frameCount);
    }

    void waitFramesToComplete(int frameCount)
    {
        std::unique_lock<std::mutex> lock(mTestMutex);

        for (int i = 0; i < frameCount; i++) {
            if (mFramesCompleted < frameCount) {
                mTestCondition.waitRelative(lock, 2 * VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mFramesCompleted == frameCount)
                    break;
            }
        }
        EXPECT_EQ(mFramesCompleted, frameCount);
    }

    void wait3AToConverge(int frameCount, bool failTestAlso = false)
    {
        waitFramesToComplete(frameCount);

        std::lock_guard<std::mutex> lock(mTestMutex);
        if (failTestAlso)
            EXPECT_EQ(m3AConverged, true);
    }

    void processBracketingRequests(camera3_capture_request_t &request,
                                   const vector<camera_metadata_t *> &settings,
                                   bool dump)
    {
        if (dump) {
            mTestMutex.lock();
            mDumpAfterFrame = mRequestsIssued + 1; // turn on dumps for next captures
            mTestMutex.unlock();
        }

        std::unique_lock<std::mutex> lock(mTestMutex);
        vector<camera_metadata_t *>::const_iterator settingIterator = settings.begin();

        while (settingIterator != settings.end()) {
            if (mBuffers.size() == 0) {
                mTestCondition.waitRelative(lock, VALGRIND_MULTIPLIER * NS_ONE_SECOND);
                if (mBuffers.size() == 0) {
                    ASSERT_TRUE(false) << "timed out waiting for buffers";
                }
            }
            mRequestsIssued++; // a bit premature, but it will assert if it fails
            request.frame_number = mRequestsIssued;
            camera3_stream_buffer streamBuffer = mBuffers[0];
            mBuffers.erase(mBuffers.begin());
            request.output_buffers = &streamBuffer;
            request.settings = *settingIterator;

            status_t status = DOPS(mDevice)->process_capture_request(CDEV(mDevice), &request);
            ASSERT_EQ(status, 0) << "Failed to issue request: status" \
                                 << std::hex <<  status;
            ++settingIterator; // next
        }
        lock.unlock();

        // wait for the requests to complete
        waitFramesToComplete(mRequestsIssued);
    }

    void runSingleStreamCapturesAndDump(camera3_capture_request_t &request,
                                        const camera_metadata_t *requestSettings,
                                        bool wait3a = true)
    {
        request.num_output_buffers = 1;
        request.input_buffer = NULL;
        request.settings = requestSettings;
        int frameCount = 0;

        if (wait3a) {
            // run 3A for this many frames
            frameCount = 400;
            PRINTLN("Running preview until 3A converges.");
            // this test runs with a low number of potentially huge buffers so it can be
            // very very slow - allow 2 seconds for frame completion
            processMultiBufferRequests(frameCount, request, true, 2);

            // wait that requests are finished
            wait3AToConverge(mRequestsIssued);
        } else {
            PRINTLN("Not waiting for 3A converging.");
        }

        mTestMutex.lock();
        mDumpAfterFrame = 0; // turn on dumps for next capture
        mFramesCompleted = 0;
        mMetaResultCompleted = 0;
        mTestMutex.unlock();

        frameCount = 1;
        processMultiBufferRequests(frameCount, request);

        // wait that requests are finished
        waitFramesToComplete(frameCount);
    }

    void TearDown() {
        mBuffers.clear();

#ifdef USE_LOCAL_BUFFER
        for (auto *address : mAllocatedBuffers) {
            free(address);
        }
#endif
        mAllocatedBuffers.clear();

        BAIL(RAWHAL_EXIT, mProgress);

RAWHAL_EXIT2:
RAWHAL_EXIT1:
        DCOMMON(mDevice).close(mDevice);
RAWHAL_EXIT0:
        return;
    }
};

class RawHal_Test : public ::Basic_Test, public ::testing::WithParamInterface<Paramz::TestParam> {
public:
    RawHal_Test() :
    mParam(NULL) {}

    void SetTestParam(Paramz::TestParam *param)
    {
        mParam = param;
        mTestWidth = param->width;
        mTestHeight = param->height;
    }

    void SetCameraConfigure(uint32_t tag, uint8_t value)
    {
        mTestCameraConfig = true;
        mMetadataTag = tag;
        mMetadataValue = value;
    }

    const Paramz::TestParam& GetParam() const
    {
        if (mParam) {
            return *mParam;
        } else {
            return ::testing::WithParamInterface<Paramz::TestParam>::GetParam();
        }
    }

    void SetUp()
    {
        Paramz::TestParam params = GetParam();
        mCameraId = params.cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        mParam = NULL;
        Basic_Test::TearDown();
    }
private:
    Paramz::TestParam *mParam;
};

void pickMaxResolutionSize(int cameraid, uint32_t format, int &width, int &height);

#endif // RAW_HAL_TEST_H
