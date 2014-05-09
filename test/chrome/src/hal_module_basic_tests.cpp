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

#include "gtest/gtest.h"
#include "test_utils.h"
#include "camera/camera_metadata.h"
#include <hardware/camera3.h>

extern camera_module_t *HAL_MODULE_INFO_SYM_PTR;
using namespace android;

TEST(OpenTest, PositiveInitDeinit) {
    hw_device_t *dev = NULL;
    int num = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();

    for (int cameraId = 0; cameraId < num; cameraId++) {
        PRINTLN("Testing camera id %d", cameraId);

        char camera[20];
        sprintf(camera, "%d", cameraId);

        HAL_MODULE_INFO_SYM_PTR->common.methods->
                open((hw_module_t *)HAL_MODULE_INFO_SYM_PTR, camera, &dev);

        ASSERT_NE(dev, (hw_device_t *) 0);
        DCOMMON(dev).close(dev);
        dev = NULL;
    }
}

TEST(OpenTest, HasStaticMetadata) {
    struct camera_info info;
    int num = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();

    for (int cameraId = 0; cameraId < num; cameraId++) {
        PRINTLN("Testing camera id %d", cameraId);

        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(cameraId, &ac2info);
        ASSERT_TRUE(ac2info.static_camera_characteristics != NULL);

        // check that there are some static metadata in it
        CameraMetadata meta;
        meta = ac2info.static_camera_characteristics;
        ASSERT_EQ(meta.isEmpty(), false);
    }
}

TEST(OpenTest, AtLeastOne) {
    int num = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();
    ASSERT_TRUE(num > 0) << "there are no cameras detected";
}

TEST(DefaultRequests, ProperPreviewRequest) {
    status_t status = OK;

    int num = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();

    for (int cameraId = 0; cameraId < num; cameraId++) {
        PRINTLN("@%s testing camera id %d", __FUNCTION__, cameraId);
        hw_device_t *dev = NULL;
        char camera[20];
        sprintf(camera, "%d", cameraId);
        HAL_MODULE_INFO_SYM_PTR->common.methods->
            open((hw_module_t *)HAL_MODULE_INFO_SYM_PTR, camera, &dev);

        ASSERT_NE(dev, (hw_device_t *) 0);

        const camera_metadata_t *request = DOPS(dev)->construct_default_request_settings(CDEV(dev),
                 CAMERA3_TEMPLATE_PREVIEW);
        ASSERT_TRUE(request != NULL);

        CameraMetadata metadata;
        metadata = request;
        ASSERT_EQ(metadata.isEmpty(), false);

        // check one entry - ANDROID_CONTROL_MODE
        camera_metadata_entry entry;
        entry = metadata.find(ANDROID_CONTROL_MODE);

        ASSERT_TRUE(entry.count == 1);
        ASSERT_TRUE(entry.data.u8[0] == ANDROID_CONTROL_MODE_AUTO);

        DCOMMON(dev).close(dev);
    }
}
