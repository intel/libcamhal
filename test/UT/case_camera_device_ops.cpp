/*
 * Copyright (C) 2015-2017 Intel Corporation.
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

#define LOG_TAG "CASE_DEVICE_OPS"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "ICamera.h"
#include "MockSysCall.h"
#include "case_common.h"


TEST_F(camHalTest, camera_hal_init_deinit_normal)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_hal_init_twice)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_open_close_normal)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);
    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_open_twice)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    ret = camera_device_open(cameraId);
    EXPECT_TRUE(ret < 0);

    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_open_close_invalid_param)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);

    ret = camera_device_open(count);
    EXPECT_TRUE(ret < 0);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_stop_without_start)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();
    ret = camera_device_open(cameraId);
    EXPECT_EQ(ret, 0);

    ret = camera_device_stop(cameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(cameraId);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_device_try_to_open_all_cameras)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);
    for (int idx = 0; idx < count; idx++) {
        ret = camera_device_open(idx);
        EXPECT_EQ(ret, 0);

        ret = camera_device_stop(idx);
        EXPECT_EQ(ret, 0);

        camera_device_close(idx);
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(camHalTest, camera_multi_process_open_the_same_device)
{
    pid_t pid=fork();
    EXPECT_TRUE(pid >= 0);

    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);

    int open_ret = camera_device_open(0);
    sleep(1);
    camera_device_close(0);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);

    if (pid == 0) {
        exit(open_ret);
    } else {
        int child_ret;
        wait(&child_ret);
        int child_open_ret = WEXITSTATUS(child_ret);
        EXPECT_TRUE((open_ret && !child_open_ret) || (!open_ret && child_open_ret));
    }
}

TEST_F(camHalTest, camera_multi_process_open_different_devices)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int count = get_number_of_cameras();
    EXPECT_TRUE(count > 0);

    if (count == 1)
        return;

    pid_t pid = fork();
    EXPECT_TRUE(pid >= 0);

    int cameraIdx;
    if (pid == 0) {
        cameraIdx = 0;
    } else {
        cameraIdx = 1;
    }

    ret = camera_device_open(cameraIdx);
    EXPECT_EQ(ret, 0);

    camera_device_close(cameraIdx);

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);

    if (pid == 0) {
        exit(0);
    } else {
        wait(NULL);
    }
}

TEST(camHalRawTest, camera_device_open_without_hal_init)
{
    int ret = camera_device_open(0);
    EXPECT_TRUE(ret < 0);
}

TEST(camHalRawTest, camera_hal_deinit_only) {
    int ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}
