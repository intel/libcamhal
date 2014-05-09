/*
 * Copyright (C) 2018 Intel Corporation
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

#ifndef PNP_HAL_TEST_H
#define PNP_HAL_TEST_H

#include <stdlib.h>

#include "raw_hal_test.h"

using namespace std;

#define PnP_HAL_TEST_DEF_FRAME_COUNT 5
#define PnP_HAL_TEST_SLEEP_SEC 1

extern int gFrameCount;

struct PnPHAL_TestParam {
    PnPHAL_TestParam(int aCameraId = 0, int aWidth = 0, int aHeight = 0,
		int aFormat = 0, int aFramerate = 0, int aframeCnt = 0) :
        cameraId(aCameraId),
        width(aWidth),
        height(aHeight),
        format(aFormat),
        framerate (aFramerate),
        frameCnt(aframeCnt) {}
    int cameraId;
    int width;
    int height;
    int format;
    int framerate;
    int frameCnt;
};

class PnPHal_Test : public ::Basic_Test, public ::testing::WithParamInterface<PnPHAL_TestParam> {
public:
    PnPHal_Test() {
        PRINTLN("@%s", __func__);
    }

    static void SetUpTestCase() {
        PRINTLN("@%s", __func__);

        // enable atrace output
        setenv(mPropCameraPerf, mValCameraPerf, 1);
        sleep(2 * PnP_HAL_TEST_SLEEP_SEC);
    }

    static void TearDownTestCase() {
        PRINTLN("@%s", __func__);

        // disable atrace output
        unsetenv(mPropCameraPerf);
        sleep(2 * PnP_HAL_TEST_SLEEP_SEC);
    }

    void SetUp() {
        PRINTLN("@%s", __func__);

        mPnPParam = ::testing::WithParamInterface<PnPHAL_TestParam>::GetParam();

        // camtune record start
        sleep(PnP_HAL_TEST_SLEEP_SEC);
        char command[256];
        snprintf(command, sizeof(command), "camtune-record start");

        FILE *fp = NULL;
        fp = popen(command, "r");
        EXPECT_TRUE(fp != NULL) << "popen ERROR: camtune-record start!";

        sleep(PnP_HAL_TEST_SLEEP_SEC);

        mCameraId = mPnPParam.cameraId;
        Basic_Test::SetUp();
    }

    void TearDown() {
        PRINTLN("@%s", __func__);

        Basic_Test::TearDown();

        // camtune record stop
        sleep(PnP_HAL_TEST_SLEEP_SEC);
        char tracename[128];
        sprintf(tracename, "%s_%dx%d_%dfps_cam%d", mTestName, mPnPParam.width,
			mPnPParam.height, mPnPParam.framerate, mPnPParam.cameraId);
        string tracename_s(tracename);
        replace(tracename_s.begin(), tracename_s.end(), '/', '-');
        char command[256];
        snprintf(command, sizeof(command), "camtune-record stop %s", tracename_s.c_str());

        FILE *fp = NULL;
        fp = popen(command, "r");
        EXPECT_TRUE(fp != NULL)  << "popen ERROR: camtune-record stop!";

        sleep(PnP_HAL_TEST_SLEEP_SEC);
    }

    static vector<PnPHAL_TestParam> getPnPHAL_TestParam(void) {
        PRINTLN("@%s", __func__);
        vector<PnPHAL_TestParam> streams;
        int framecnt = (gFrameCount < 0) ? PnP_HAL_TEST_DEF_FRAME_COUNT : gFrameCount;

        // workload for front camera
        streams.push_back(PnPHAL_TestParam(1, 3264, 2448, HAL_PIXEL_FORMAT_YCbCr_420_888, 20, framecnt));
        streams.push_back(PnPHAL_TestParam(1, 1640, 1232, HAL_PIXEL_FORMAT_YCbCr_420_888, 30, framecnt));
        streams.push_back(PnPHAL_TestParam(1, 1920, 1080, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 30, framecnt));
        streams.push_back(PnPHAL_TestParam(1, 1280, 720,  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 120, framecnt));

        // workload for back camera
        streams.push_back(PnPHAL_TestParam(0, 3264, 2448, HAL_PIXEL_FORMAT_YCbCr_420_888, 20, framecnt));
        streams.push_back(PnPHAL_TestParam(0, 1640, 1232, HAL_PIXEL_FORMAT_YCbCr_420_888, 30, framecnt));
        streams.push_back(PnPHAL_TestParam(0, 1920, 1080, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 30, framecnt));
        streams.push_back(PnPHAL_TestParam(0, 1280, 720,  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED, 120, framecnt));

        return streams;
    }

public:
    struct PnPHAL_TestParam mPnPParam;

private:
    static const char* mPropCameraPerf;
    static const char* mValCameraPerf;
};

const char* PnPHal_Test::mPropCameraPerf  = "cameraPerf";
const char* PnPHal_Test::mValCameraPerf = "16";

#endif
