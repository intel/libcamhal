/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#define LOG_TAG "Camera3HAL"

#include "Utils.h"
#include "Errors.h"
#include "ICamera.h"

#include "Camera3HAL.h"
#include "HALv3Utils.h"

namespace camera3 {

/******************************************************************************
 *  C DEVICE INTERFACE IMPLEMENTATION WRAPPER
 *****************************************************************************/

//Common check before the function call
#define FUNCTION_PREPARED_RETURN \
    if (!dev)\
        return -EINVAL;\
    Camera3HAL* camera_priv = static_cast<Camera3HAL*>(dev->priv);

static int
hal_dev_initialize(const struct camera3_device * dev,
                   const camera3_callback_ops_t *callback_ops)
{
    LOG1("@%s", __func__);

    FUNCTION_PREPARED_RETURN

    return camera_priv->initialize(callback_ops);
}

static int
hal_dev_configure_streams(const struct camera3_device * dev,
                          camera3_stream_configuration_t *stream_list)
{
    LOG1("@%s", __func__);

    FUNCTION_PREPARED_RETURN

    return camera_priv->configure_streams(stream_list);
}

static const camera_metadata_t*
hal_dev_construct_default_request_settings(const struct camera3_device * dev,
                                           int type)
{
    LOG1("@%s", __func__);

    if (!dev)
        return nullptr;
    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);

    return camera_priv->construct_default_request_settings(type);
}

static int
hal_dev_process_capture_request(const struct camera3_device * dev,
                                camera3_capture_request_t *request)
{
    LOG1("@%s", __func__);

    FUNCTION_PREPARED_RETURN

    return camera_priv->process_capture_request(request);
}

static void
hal_dev_dump(const struct camera3_device * dev, int fd)
{
    LOG1("@%s", __func__);

    if (!dev)
        return;

    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);

    camera_priv->dump(fd);
}

static int
hal_dev_flush(const struct camera3_device * dev)
{
    LOG1("@%s", __func__);

    if (!dev)
        return -EINVAL;

    Camera3HAL* camera_priv = (Camera3HAL*)(dev->priv);
    return camera_priv->flush();
}

static camera3_device_ops hal_dev_ops = {
    .initialize                         = hal_dev_initialize,
    .configure_streams                  = hal_dev_configure_streams,
    .register_stream_buffers            = nullptr,
    .construct_default_request_settings = hal_dev_construct_default_request_settings,
    .process_capture_request            = hal_dev_process_capture_request,
    .get_metadata_vendor_tag_ops        = nullptr,
    .dump                               = hal_dev_dump,
    .flush                              = hal_dev_flush,
    .reserved                           = {0},
};

/******************************************************************************
 *  C++ CLASS IMPLEMENTATION
 *****************************************************************************/
Camera3HAL::Camera3HAL(int cameraId, const hw_module_t* module) :
    mCameraId(cameraId)
{
    LOG1("@%s", __func__);

    memset(&mDevice, 0, sizeof(mDevice));
    mDevice.common.tag = HARDWARE_DEVICE_TAG;
    mDevice.common.version = CAMERA_DEVICE_API_VERSION_3_3;
    mDevice.common.module = (hw_module_t *)(module);
    // hal_dev_close is kept in the module for symmetry with dev_open
    // it will be set there
    mDevice.common.close = nullptr;
    mDevice.ops = &hal_dev_ops;
    mDevice.priv = this;

    mRequestManager = new RequestManager(cameraId);
}

Camera3HAL::~Camera3HAL()
{
    LOG1("@%s", __func__);
    mDevice.priv = nullptr;

    delete mRequestManager;
}

int Camera3HAL::init(void)
{
    LOG1("@%s", __func__);

    int status = icamera::camera_hal_init();
    Check(status != icamera::OK, status, "failed to init icamera, status %d", status);

    status = icamera::camera_device_open(mCameraId);
    if (status != icamera::OK) {
        LOGE("failed to open device, status %d", status);
        goto bail;
    }

    return icamera::OK;

bail:
    deinit();
    return status;
}

int Camera3HAL::deinit(void)
{
    LOG1("@%s", __func__);

    // flush request first
    int status = mRequestManager->flush();

    status |= mRequestManager->deinit();

    icamera::camera_device_close(mCameraId);
    icamera::camera_hal_deinit();

    return status;
}

/* *********************************************************************
 * Camera3 device  APIs
 * ********************************************************************/
int Camera3HAL::initialize(const camera3_callback_ops_t *callback_ops)
{
    LOG1("@%s", __func__);
    int status = icamera::OK;

    if (callback_ops == nullptr)
        return -ENODEV;

    status = mRequestManager->init(callback_ops);
    if (status != icamera::OK) {
        LOGE("Error register callback status = %d", status);
        return -ENODEV;
    }
    return status;
}

int Camera3HAL::configure_streams(camera3_stream_configuration_t *stream_list)
{
    LOG1("@%s", __func__);
    if (!stream_list)
        return -EINVAL;

    if (!stream_list->streams || !stream_list->num_streams) {
        LOGE("%s: Bad input! streams list ptr: %p, num %d", __func__,
            stream_list->streams, stream_list->num_streams);
        return -EINVAL;
    }
    int num = stream_list->num_streams;
    LOG2("@%s, line:%d, stream num:%d", __func__, __LINE__, num);
    while (num--) {
        if (!stream_list->streams[num]){
            LOGE("%s: Bad input! streams (%d) 's ptr: %p", __func__,
                num, stream_list->streams[num]);
            return -EINVAL;
        }
    }

    int status = mRequestManager->configureStreams(stream_list);
    return (status == icamera::OK) ? 0 : -EINVAL;
}

const camera_metadata_t* Camera3HAL::construct_default_request_settings(int type)
{
    LOG1("@%s, type:%d", __func__, type);
    const camera_metadata_t * meta;

    if (type < CAMERA3_TEMPLATE_PREVIEW || type >= CAMERA3_TEMPLATE_COUNT)
        return nullptr;

    int status = mRequestManager->constructDefaultRequestSettings(type, &meta);
    Check(status != icamera::OK, nullptr, "construct default request setting error");

    return meta;
}

int Camera3HAL::process_capture_request(camera3_capture_request_t *request)
{
    LOG2("@%s", __func__);
    if (request == nullptr) {
        LOGE("%s: request is null!", __func__);
        return -EINVAL;
    } else if (!request->num_output_buffers || request->output_buffers == nullptr) {
        LOGE("%s: num_output_buffers %d, output_buffers %p", __func__,
              request->num_output_buffers, request->output_buffers);
        return -EINVAL;
    } else if (request->output_buffers->stream == nullptr) {
        LOGE("%s: output_buffers->stream is null!", __func__);
        return -EINVAL;
    } else if (request->output_buffers->stream->priv == nullptr) {
        LOGE("%s: output_buffers->stream->priv is null!", __func__);
        return -EINVAL;
    } else if (request->output_buffers->buffer == nullptr
          || *(request->output_buffers->buffer) == nullptr) {
        LOGE("%s: output buffer is invalid", __func__);
        return -EINVAL;
    }

    int status = mRequestManager->processCaptureRequest(request);
    if (status == icamera::OK)
        return icamera::OK;

    return (status == icamera::BAD_VALUE) ? -EINVAL : -ENODEV;
}

void Camera3HAL::dump(int fd)
{
    LOG1("@%s", __func__);

    mRequestManager->dump(fd);
}

int Camera3HAL::flush()
{
    LOG1("@%s", __func__);

    return mRequestManager->flush();
}

} // namespace camera3
