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

#define LOG_TAG "CASE_AIQ"

#include "iutils/CameraLog.h"

#include "MockSysCall.h"
#include "case_common.h"
#include "AiqCore.h"
#include "AiqUtils.h"
// INTEL_DVS_S
#include "IntelDvs.h"
// INTEL_DVS_E
#include "PlatformData.h"
#include "AiqResultStorage.h"

#include "LensHw.h"
#include "SensorHwCtrl.h"
#include "I3AControlFactory.h"

#include "aiq_stats_test_data.h"

using namespace icamera;

static const unsigned int AIQ_RUN_TIMES = 20;
static const unsigned int AIQ_RESULT_NUM = 5;
static const unsigned int FRAME_ID_START_COUNT = 1;
static unsigned long long FRAME_TIMESTAMP = 1150409707; // microsecond unit
static const unsigned int FRAME_DURATION = 33000; // microsecond unit


/**
 * To run all aiq related case, add --gtest_filter="*aiq*"
 */

/**
 * Test if aiq run correctly from AIQ unit API level
 */
TEST(camHalAiqTest, i3a_control_run_aiq_test) {
    int cameraNum = PlatformData::numberOfCameras();
    int cameraId = getCurrentCameraId();

    if (!PlatformData::isEnableAIQ(cameraId)) {
        PlatformData::releaseInstance();
        return;
    }

    LensHw *lensHw = new LensHw(cameraId);
    SensorHwCtrl *sensorHw = SensorHwCtrl::createSensorCtrl(cameraId);
    AiqUnitBase *control = I3AControlFactory::createI3AControl(cameraId, sensorHw, lensHw);

    Parameters parameter;

    camera_info_t info;
    CLEAR(info);
    PlatformData::getCameraInfo(cameraId, info);

    parameter = *info.capability;

    camera_range_t fps = {10, 60};
    parameter.setFpsRange(fps);
    parameter.setFrameRate(30);

    camera_image_enhancement_t enhancement;
    CLEAR(enhancement); // All use 0 as default
    parameter.setImageEnhancement(enhancement);
    parameter.setWeightGridMode(WEIGHT_GRID_AUTO);
    parameter.setWdrLevel(100);
    parameter.setYuvColorRangeMode(PlatformData::getYuvColorRangeMode(cameraId));

    int ret = control->init();
    EXPECT_EQ(ret, OK);

    vector<EventListener*> statsListenerList = control->getStatsEventListener();
    EXPECT_NE(statsListenerList.size(), 0);
    vector<EventListener*> sofListenerList = control->getSofEventListener();
    EXPECT_NE(sofListenerList.size(), 0);

    stream_config_t streamList;
    streamList.num_streams = 1;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    stream_t streams[streamList.num_streams];
    streamList.streams = streams;
    streams[0].usage = CAMERA_STREAM_PREVIEW;
    streams[0].width = 1920;
    streams[0].height = 1080;
    ret = control->configure(&streamList);
    EXPECT_EQ(ret, OK);

    ret = control->start();
    EXPECT_EQ(ret, OK);

    ret = control->setParameters(parameter);
    EXPECT_EQ(ret, OK);

    long sequence = -1;
    ret = control->run3A(&sequence);
    EXPECT_EQ(ret, OK);
    EXPECT_NE(sequence, -1);

    ret = control->stop();
    EXPECT_EQ(ret, OK);

    ret = control->deinit();
    EXPECT_EQ(ret, OK);

    delete control;
    delete sensorHw;
    delete lensHw;

    PlatformData::releaseInstance();
}

/**
 * Test if aiq can be run correctly
 */
TEST(camHalAiqTest, aiq_core_run_aiq_test) {
    int cameraNum = PlatformData::numberOfCameras();
    for (int cameraId = 0; cameraId < cameraNum; cameraId++) {
        if (!PlatformData::isEnableAIQ(cameraId)) continue;

        int ret = OK;
        AiqCore *aiqCore = new AiqCore(cameraId);
        AiqResult *aiqResult[AIQ_RESULT_NUM], *lastAiqResult = NULL;
        for (int i = 0; i < AIQ_RESULT_NUM; ++i) {
            aiqResult[i] = new AiqResult(cameraId);
            ret = aiqResult[i]->init();
            EXPECT_EQ(ret, OK);
        }

        ret = aiqCore->init();
        EXPECT_EQ(ret, OK);

        vector<ConfigMode> configModes;
        PlatformData::getConfigModesByOperationMode(cameraId, CAMERA_STREAM_CONFIGURATION_MODE_AUTO, configModes);
        ret = aiqCore->configure(configModes);
        EXPECT_EQ(ret, OK);

        ret = aiqCore->setSensorInfo(ov5693_frame_params, ov5693_sensor_descriptor);
        EXPECT_EQ(ret, OK);

        ia_aiq_statistics_input_params_v4 statsParam;
        CLEAR(statsParam);

        const ia_aiq_rgbs_grid* rgbsGridArray[MAX_EXPOSURES_NUM];
        const ia_aiq_af_grid* afGridArray[MAX_EXPOSURES_NUM];

        unsigned int aiqResultIndex = 0;
        unsigned int aiqStatsIndex = 0;

        ia_aiq_rgbs_grid rgbs;
        rgbs.grid_width = ov5693_rgbs_grid[aiqStatsIndex].grid_width;
        rgbs.grid_height = ov5693_rgbs_grid[aiqStatsIndex].grid_height;
        rgbs.blocks_ptr = new rgbs_grid_block[rgbs.grid_width * rgbs.grid_height];

        static int frame_id = FRAME_ID_START_COUNT;
        static unsigned long long frame_timestamp = FRAME_TIMESTAMP;

        bool isHDR = false;
        vector <TuningConfig> configs;
        PlatformData::getSupportedTuningConfig(cameraId, configs);
        if (configs.empty()) return;
        TuningConfig *tuningConfig = &configs[0];

        isHDR = CameraUtils::isHdrPsysPipe(tuningConfig->tuningMode);

        int exposureNum = PlatformData::getExposureNum(cameraId, isHDR);

        for (int index = 0; index < AIQ_RUN_TIMES; ++index) {
            statsParam.rgbs_grids = rgbsGridArray;
            statsParam.af_grids = afGridArray;

            rgbs_grid_block* dstBlock = rgbs.blocks_ptr;
            rgbs_grid_block* srcBlock = ov5693_rgbs_grid[aiqStatsIndex].blocks_ptr;

            for (int i = 0; i < rgbs.grid_width * rgbs.grid_height; i++) {
                dstBlock->avg_gr = srcBlock[i].avg_gr;
                dstBlock->avg_r = srcBlock[i].avg_r;
                dstBlock->avg_b = srcBlock[i].avg_b;
                dstBlock->avg_gb = srcBlock[i].avg_gb;
                dstBlock->sat = srcBlock[i].sat;
                if (i%100 == 0) {
                    LOG3A("i = %d, [%d, %d, %d, %d, %d]", i, dstBlock->avg_gr, dstBlock->avg_r,
                                             dstBlock->avg_b, dstBlock->avg_gb, dstBlock->sat);
                }
                dstBlock++;
            }

            if (lastAiqResult != NULL) {
                statsParam.frame_id = frame_id;
                statsParam.frame_timestamp = frame_timestamp;
                statsParam.frame_ae_parameters = &lastAiqResult->mAeResults;
                statsParam.frame_af_parameters = &lastAiqResult->mAfResults;
                for (int i = 0; i < exposureNum; i++) {
                    statsParam.rgbs_grids[i] = &rgbs;
                }
                statsParam.num_rgbs_grids = exposureNum;
                statsParam.af_grids[0] = NULL;
                statsParam.num_af_grids = 0;
                statsParam.external_histograms = NULL;
                statsParam.num_external_histograms = 0;
                statsParam.frame_pa_parameters = &lastAiqResult->mPaResults;
                statsParam.faces = NULL;
                statsParam.camera_orientation = ia_aiq_camera_orientation_unknown;
                statsParam.awb_results = &lastAiqResult->mAwbResults;
                statsParam.frame_sa_parameters = &lastAiqResult->mSaResults;

                ret = aiqCore->setStatistics(&statsParam);
                EXPECT_EQ(ret, OK);
            }

            frame_id++;
            frame_timestamp += FRAME_DURATION;

            aiq_parameter_t param;
            param.tuningMode = tuningConfig->tuningMode;
            aiqCore->updateParameter(param);

            ret = aiqCore->runAiq(aiqResult[aiqResultIndex]);
            EXPECT_EQ(ret, OK);

            lastAiqResult = aiqResult[aiqResultIndex];

            ++aiqResultIndex;
            aiqResultIndex %= AIQ_RESULT_NUM;

            ++aiqStatsIndex;
            aiqStatsIndex %= AIQ_STATS_NUM;
        }

        delete [] rgbs.blocks_ptr;
        delete aiqCore;
        for (int i = 0; i < AIQ_RESULT_NUM; ++i) {
            aiqResult[i]->deinit();
            delete aiqResult[i];
        }
    }

    PlatformData::releaseInstance();
}

TEST(camRawTest, test_coodinate_system_conversion) {
    camera_coordinate_system_t srcSystem = {0, 0, 1000, 1000};
    camera_coordinate_system_t dstSystem = {0, 0, 2000, 2000};
    camera_coordinate_t srcPoint = {100, 200};
    camera_coordinate_t dstPoint = AiqUtils::convertCoordinateSystem(srcSystem, dstSystem, srcPoint);
    EXPECT_EQ(200, dstPoint.x);
    EXPECT_EQ(400, dstPoint.y);
}

TEST(camRawTest, test_convert_ia_coodinate_system) {
    camera_coordinate_system_t srcSystem = {0, 0, 1024, 1024};
    camera_coordinate_t srcPoint = {100, 200};
    camera_coordinate_t dstPoint = AiqUtils::convertToIaCoordinate(srcSystem, srcPoint);
    EXPECT_EQ(800, dstPoint.x);
    EXPECT_EQ(1600, dstPoint.y);
}

void verifyAiqResultData(int sequeceId, const AiqResult* result)
{
    EXPECT_NOT_NULL(result);
    if (result != NULL) {
        EXPECT_EQ(sequeceId, result->mSequence);
        EXPECT_EQ(sequeceId * 100, result->mAeResults.exposures->exposure->exposure_time_us);
        EXPECT_EQ(sequeceId * 10, result->mAeResults.exposures->exposure->analog_gain);
    }
}

TEST(camRawTest, test_aiq_result_set_and_get) {
    int cameraNum = PlatformData::numberOfCameras();
    for (int cameraId = 0; cameraId < cameraNum; cameraId++) {
        if (!PlatformData::isEnableAIQ(cameraId)) continue;

        AiqResultStorage* storage = AiqResultStorage::getInstance(cameraId);

        AiqResult *resultIn = NULL;

        // Make exposure_time_us equal 100 times of sequence id and analog_gain equal 10 times of
        // sequence id for test convenience.
        for (int i = 1; i <= 5; i++) {
            resultIn = storage->acquireAiqResult();
            resultIn->mSequence = i;
            resultIn->mAeResults.num_exposures = 1;
            resultIn->mAeResults.exposures->exposure->exposure_time_us = i * 100;
            resultIn->mAeResults.exposures->exposure->analog_gain = i * 10;
            storage->updateAiqResult(resultIn->mSequence);
        }

        const AiqResult* resultOut = storage->getAiqResult();
        verifyAiqResultData(5, resultOut);

        resultOut = storage->getAiqResult(1);
        verifyAiqResultData(1, resultOut);

        resultOut = storage->getAiqResult(6);
        verifyAiqResultData(5, resultOut); // Suppose same as 5 due to no new result applied.

        for (int i = 6; i <= 100; i += 2) {
            resultIn = storage->acquireAiqResult();
            resultIn->mSequence = i;
            resultIn->mAeResults.num_exposures = 1;
            resultIn->mAeResults.exposures->exposure->exposure_time_us = i * 100;
            resultIn->mAeResults.exposures->exposure->analog_gain = i * 10;
            storage->updateAiqResult(resultIn->mSequence);
        }

        resultOut = storage->getAiqResult();
        verifyAiqResultData(100, resultOut);

        resultOut = storage->getAiqResult(100);
        verifyAiqResultData(100, resultOut);

        resultOut = storage->getAiqResult(99);
        verifyAiqResultData(98, resultOut); // Suppose same as 98 due to no new result applied.

        resultOut = storage->getAiqResult(98);
        verifyAiqResultData(98, resultOut);

        resultOut = storage->getAiqResult(50);
        EXPECT_NULL(resultOut); // Expect not to save old data.

        AiqResultStorage::releaseAiqResultStorage(cameraId);
    }

    PlatformData::releaseInstance();
}

// INTEL_DVS_S
TEST(camRawTest, test_aiq_dvs_class_api) {
    int cameraNum = PlatformData::numberOfCameras();
    for (int cameraId = 0; cameraId < cameraNum; cameraId++) {

        // Only test for DVS type MORPH_TABLE for now.
        if (PlatformData::getDVSType(cameraId) != MORPH_TABLE) continue;

        vector <TuningConfig> configs;
        PlatformData::getSupportedTuningConfig(cameraId, configs);
        if (configs.empty()) continue;

        IntelDvs dvs(cameraId);

        int ret = dvs.init();
        EXPECT_EQ(ret, OK);

        vector<ConfigMode> configModes;
        configModes.push_back(configs[0].configMode);
        ret = dvs.configure(configModes, 1, 1920, 1080);
        EXPECT_EQ(ret, OK);

        ia_aiq_ae_results aeResults;
        CLEAR(aeResults);
        DvsResult result;

        ret = dvs.run(aeResults, &result, 0, 0);
        EXPECT_EQ(ret, OK);

        ret = dvs.deinit();
        EXPECT_EQ(ret, OK);
    }

    PlatformData::releaseInstance();
}
// INTEL_DVS_E

TEST(camRawTest, test_digital_gain_api) {
    int ret = OK;
    int testCount = 50;
    const float GAIN_TOLERANCE = 0.05f;
    int cameraNum = PlatformData::numberOfCameras();

    for (int cameraId = 0; cameraId < cameraNum; cameraId++) {

        if (!PlatformData::isUsingIspDigitalGain(cameraId))
            continue;

        SensorDgType dgType = PlatformData::sensorDigitalGainType(cameraId);
        if (dgType == SENSOR_DG_TYPE_NONE) {
            LOGE("cameraId: %d, don't support the sensor digital gain type: %d", cameraId, dgType);
            ret = INVALID_OPERATION;
            break;
        }

        for (int i = 0; i < testCount; i++) {
            float realDg = (float)getRandomValue(1, 256);
            int sensorDg = AiqUtils::getSensorDigitalGain(cameraId, realDg);
            float ispDg = AiqUtils::getIspDigitalGain(cameraId, realDg);

            if(dgType == SENSOR_DG_TYPE_2_X) {
                float getRealDg = pow(2, sensorDg) * ispDg;
                if(fabs(getRealDg - realDg) >= GAIN_TOLERANCE) {
                    ret = BAD_VALUE;
                    break;
                }
            }
        }
        if (ret != OK) {
            LOGE("cameraId: %d, the calculation api for separating gain isn't correct", cameraId);
            break;
        }
    }
    EXPECT_EQ(ret, OK);

    PlatformData::releaseInstance();
}
