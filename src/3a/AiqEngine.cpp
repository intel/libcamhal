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

#define LOG_TAG "AiqEngine"

#include <iutils/Utils.h>
#include <iutils/Errors.h>
#include <iutils/CameraLog.h>

#include "PlatformData.h"
#include "AiqEngine.h"

namespace icamera {

AiqEngine::AiqEngine(int cameraId, SensorHwCtrl *sensorHw, LensHw *lensHw, AiqSetting *setting) :
    mCameraId(cameraId),
    mAiqSetting(setting),
    mFirstAiqRunning(true),
    mFirstExposureSetting(true),
    mAiqRunningForPerframe(false),
    m3ACadenceSequence(0),
    mLastStatsSequence(-1)
{
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    CLEAR(mRgbsGridArray);
    CLEAR(mAfGridArray);

    mIntelMkn = IntelMkn::getInstance(mCameraId);

    mAiqCore = new AiqCore(mCameraId);

    mSensorManager = new SensorManager(mCameraId, sensorHw);

    mLensManager = new LensManager(mCameraId, lensHw);

    // Should consider better place to maintain the life cycle of AiqResultStorage
    mAiqResultStorage = AiqResultStorage::getInstance(mCameraId);
    mCurrentStatsSequence = 0;
}

AiqEngine::~AiqEngine()
{
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    delete mLensManager;

    delete mSensorManager;

    delete mAiqCore;

    AiqResultStorage::releaseAiqResultStorage(mCameraId);

    IntelMkn::releaseIntelMkn(mCameraId);
}

int AiqEngine::init()
{
    AutoMutex l(mEngineLock);
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    if (mAiqCore->init() != OK) {
        return UNKNOWN_ERROR;
    }

    mSensorManager->init();

    LOG1("%s, end mCameraId = %d", __func__, mCameraId);
    return OK;
}

int AiqEngine::deinit()
{
    AutoMutex l(mEngineLock);
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    mSensorManager->deinit();

    mAiqCore->deinit();

    LOG1("%s, end mCameraId = %d", __func__, mCameraId);
    return OK;
}

int AiqEngine::configure(const vector<ConfigMode>& configModes)
{
    AutoMutex l(mEngineLock);
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    mAiqCore->configure(configModes);

    return OK;
}

int AiqEngine::startEngine()
{
    AutoMutex l(mEngineLock);
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    mFirstAiqRunning = true;
    mFirstExposureSetting = true;

    mSensorManager->reset();

    mLensManager->start();

    m3ACadenceSequence = 0;

    LOG1("%s, end mCameraId = %d", __func__, mCameraId);

    return OK;
}

int AiqEngine::stopEngine()
{
    AutoMutex l(mEngineLock);
    LOG1("%s, end mCameraId = %d", __func__, mCameraId);

    mLensManager->stop();

    return OK;
}

int AiqEngine::run3A(long *settingSequence)
{
    LOG3A("%s", __func__);
    // Run 3A in call thread
    AutoMutex l(mEngineLock);

    // Handle 3A cadence logic
    int run3ACadence = mAiqParam.run3ACadence;
    if (run3ACadence < 1) {
        LOGW("Invalid 3A cadence %d, use default 1.", run3ACadence);
        run3ACadence = 1;
    }
    LOG2("%s: run3ACadence is %d", __func__, run3ACadence);

    if (m3ACadenceSequence % run3ACadence != 0) {
        // Skip 3A per cadence
        m3ACadenceSequence ++;
        return OK;
    }
    LOG2("%s: run 3A for cadence sequence %ld", __func__, m3ACadenceSequence);
    m3ACadenceSequence ++;

    mAiqRunningForPerframe = (settingSequence != nullptr);
    AiqState state = prepareInputParam();

    AiqResult *aiqResult = mAiqResultStorage->acquireAiqResult();
    aiqResult->mTuningMode = mAiqParam.tuningMode;

    if (state == AIQ_STATE_RUN) {
        state = runAiq(aiqResult);
    }
    if (state == AIQ_STATE_RESULT_SET) {
        state = handleAiqResult(aiqResult);
    }
    if (state == AIQ_STATE_DONE) {
        done(aiqResult);
    }

    mAiqResultStorage->unLockAiqStatistics();

    if (settingSequence) {
        *settingSequence = mAiqResultStorage->getAiqResult()->mSequence;
        LOG3A("%s, sequence %ld, mLastStatsSequence %ld", __func__, *settingSequence,
               mLastStatsSequence);
    }

    mIntelMkn->saveMakernoteData(mAiqParam.makernoteMode,
                                 mAiqResultStorage->getAiqResult()->mSequence);

    return (state == AIQ_STATE_DONE || state == AIQ_STATE_WAIT) ? 0 : UNKNOWN_ERROR;
}

EventListener *AiqEngine::getSofEventListener()
{
    AutoMutex l(mEngineLock);
    LOG1("%s, mCameraId = %d", __func__, mCameraId);

    return mSensorManager->getSofEventListener();
}

int AiqEngine::saveAfGridData(const ia_aiq_af_grid* afGrid, ia_aiq_af_grid* dst)
{
    LOG3A("%s", __func__);
    if (afGrid == nullptr
        || afGrid->filter_response_1 == nullptr
        || afGrid->filter_response_2 == nullptr
        || afGrid->grid_width == 0
        || afGrid->grid_height == 0) {
        LOGE("%s, af grids are invalid", __func__);
        return BAD_VALUE;
    }

    size_t gridSize = afGrid->grid_width * afGrid->grid_height;
    if (afGrid->grid_width != dst->grid_width || afGrid->grid_height != dst->grid_height) {
        if (dst->filter_response_1 != nullptr) {
            delete [] dst->filter_response_1;
        }
        dst->filter_response_1 = new int[gridSize];
        if (dst->filter_response_2 != nullptr) {
            delete [] dst->filter_response_2;
        }
        dst->filter_response_2 = new int[gridSize];
    }

    dst->grid_width = afGrid->grid_width;
    dst->grid_height = afGrid->grid_height;
    dst->block_width = afGrid->block_width;
    dst->block_height = afGrid->block_height;
    MEMCPY_S(dst->filter_response_1, gridSize * sizeof(int),
             afGrid->filter_response_1, gridSize * sizeof(int));
    MEMCPY_S(dst->filter_response_2, gridSize * sizeof(int),
             afGrid->filter_response_2, gridSize * sizeof(int));

    LOG3A("%s, grid size=[%dx%d]", __func__, dst->grid_width, dst->grid_height);
    return OK;
}

int AiqEngine::saveRgbsGridData(const ia_aiq_rgbs_grid* rgbsGrid, ia_aiq_rgbs_grid* dst)
{
    LOG3A("%s", __func__);
    if (rgbsGrid == nullptr
        || rgbsGrid->blocks_ptr == nullptr
        || rgbsGrid->grid_width == 0
        || rgbsGrid->grid_height == 0) {
        LOGE("%s, rgbs grids are invalid", __func__);
        return BAD_VALUE;
    }

    size_t gridSize = rgbsGrid->grid_width * rgbsGrid->grid_height;
    if (rgbsGrid->grid_width != dst->grid_width || rgbsGrid->grid_height != dst->grid_height) {
        if (dst->blocks_ptr != nullptr) {
            delete [] dst->blocks_ptr;
        }
        dst->blocks_ptr = new rgbs_grid_block[gridSize];
    }

    dst->grid_width = rgbsGrid->grid_width;
    dst->grid_height = rgbsGrid->grid_height;
    MEMCPY_S(dst->blocks_ptr, gridSize * sizeof(rgbs_grid_block),
             rgbsGrid->blocks_ptr, gridSize * sizeof(rgbs_grid_block));

    dst->shading_correction = rgbsGrid->shading_correction;

    LOG3A("%s, grid size=[%dx%d]", __func__, dst->grid_width, dst->grid_height);
    return OK;
}

int AiqEngine::saveHdrRgbsGridData(const ia_aiq_hdr_rgbs_grid* rgbsGrid, ia_aiq_hdr_rgbs_grid* dst)
{
    LOG3A("%s", __func__);
    if (rgbsGrid == nullptr
        || rgbsGrid->blocks_ptr == nullptr
        || rgbsGrid->grid_width == 0
        || rgbsGrid->grid_height == 0) {
        LOGE("%s, rgbs grids are invalid", __func__);
        return BAD_VALUE;
    }

    size_t gridSize = rgbsGrid->grid_width * rgbsGrid->grid_height;
    if (rgbsGrid->grid_width != dst->grid_width || rgbsGrid->grid_height != dst->grid_height) {
        if (dst->blocks_ptr != nullptr) {
            delete [] dst->blocks_ptr;
        }
        dst->blocks_ptr = new hdr_rgbs_grid_block[gridSize];
    }

    dst->grid_width = rgbsGrid->grid_width;
    dst->grid_height = rgbsGrid->grid_height;
    MEMCPY_S(dst->blocks_ptr, gridSize * sizeof(hdr_rgbs_grid_block),
             rgbsGrid->blocks_ptr, gridSize * sizeof(hdr_rgbs_grid_block));

    LOG3A("%s, grid size=[%dx%d]", __func__, dst->grid_width, dst->grid_height);
    return OK;
}

int AiqEngine::prepareStats(ia_aiq_statistics_input_params_v4 *statsParam,
                            ia_aiq_gbce_results *gbceResults,
                            AiqStatistics *aiqStatistics)
{
    mLastStatsSequence = aiqStatistics->mSequence;
    LOG3A("%s, sequence %ld", __func__, aiqStatistics->mSequence);

    statsParam->rgbs_grids = mRgbsGridArray;
    statsParam->af_grids = mAfGridArray;

    int ret = OK;
    do {
        // Run 3A without statistics when switching pipe.
        if (mAiqParam.tuningMode == TUNING_MODE_VIDEO_ULL
                && aiqStatistics->mTuningMode == TUNING_MODE_VIDEO_HDR) {
            LOG2("Switching from HDR to ULL pipe");
            ret = INVALID_OPERATION;
            break;
        } else if (mAiqParam.tuningMode == TUNING_MODE_VIDEO_HDR
                && aiqStatistics->mTuningMode == TUNING_MODE_VIDEO_ULL) {
            LOG2("Switching from ULL to HDR pipe");
            ret = INVALID_OPERATION;
            break;
        }

        // The statistics timestamp is incorrect. If possible, use SOF timestamp instead.
        unsigned long long timestamp = mSensorManager->getSofTimestamp(aiqStatistics->mSequence);
        if (timestamp == 0) {
            LOG2("The sof sequence was not found %ld", aiqStatistics->mSequence);
            timestamp = aiqStatistics->mTimestamp;
        }

        statsParam->frame_id = aiqStatistics->mSequence;
        statsParam->frame_timestamp = timestamp;
        statsParam->num_rgbs_grids = PlatformData::getExposureNum(mCameraId,
                                              CameraUtils::isHdrPsysPipe(mAiqParam.tuningMode));

        if (statsParam->num_rgbs_grids > 1) {
            for (unsigned int i = 0; i < statsParam->num_rgbs_grids; i++) {
                statsParam->rgbs_grids[i] = &(aiqStatistics->mRgbsGridArray[i]);
            }
        } else {
            statsParam->rgbs_grids[0] = &(aiqStatistics->mRgbsGridArray[0]);
        }
        statsParam->hdr_rgbs_grid = &aiqStatistics->mHdrRgbsGrid;
        statsParam->af_grids[0] = &(aiqStatistics->mAfGridArray[0]);
        statsParam->num_af_grids = 1;
        statsParam->external_histograms = nullptr;
        statsParam->num_external_histograms = 0;
        statsParam->faces = nullptr;
        statsParam->camera_orientation = ia_aiq_camera_orientation_unknown;

        const AiqResult *feedback = mAiqResultStorage->getAiqResult(aiqStatistics->mSequence);
        if (feedback == nullptr) {
            LOGW("%s: no feed back result for sequence %ld! use the latest instead",
                    __func__, aiqStatistics->mSequence);
            feedback = mAiqResultStorage->getAiqResult();
        }

        statsParam->frame_ae_parameters = &feedback->mAeResults;
        statsParam->frame_af_parameters = &feedback->mAfResults;
        statsParam->frame_pa_parameters = &feedback->mPaResults;
        statsParam->awb_results = &feedback->mAwbResults;
        statsParam->frame_sa_parameters = &feedback->mSaResults;

        *gbceResults = feedback->mGbceResults;
    } while (0);
    LOG3A("%s end", __func__);
    return ret;
}

void AiqEngine::setAiqResult(AiqResult *aiqResult, bool skip)
{
    SensorExpGroup sensorExposures;
    for (unsigned int i = 0; i < aiqResult->mAeResults.num_exposures; i++) {
        SensorExposure exposure;
        exposure.sensorParam = *aiqResult->mAeResults.exposures[i].sensor_exposure;
        exposure.realDigitalGain = (aiqResult->mAeResults.exposures[i].exposure)->digital_gain;
        sensorExposures.push_back(exposure);
    }
    bool useSof = !mFirstExposureSetting;
    aiqResult->mSequence = mSensorManager->updateSensorExposure(sensorExposures, useSof);
    if (mFirstExposureSetting) {
        mFirstExposureSetting = false;
    }
    aiqResult->mSkip = skip;

    if (skip) {
        LOG3A("%s, skipping frame aiqResult->mSequence = %ld", __func__, aiqResult->mSequence);
    }

    long sequence = aiqResult->mSequence - PlatformData::getExposureLag(mCameraId);
    mSensorManager->setWdrMode(aiqResult->mTuningMode, sequence);

    mLensManager->setLensResult(aiqResult->mAeResults, aiqResult->mAfResults);

    aiqResult->mAiqParam = mAiqParam;
}

int AiqEngine::getSkippingNum(AiqResult *aiqResult)
{
    LOG3A("%s", __func__);
    int skipNum = 0;

    if (!mFirstAiqRunning) {
        const AiqResult *lastResult = mAiqResultStorage->getAiqResult();
        if (lastResult->mTuningMode != aiqResult->mTuningMode) {
            // Skip 3 frames when pipe switching
            skipNum = 3;
        }
    } else if (mAiqRunningForPerframe) {
        /** Handle initial skip:
         * The 1st result takes effect @ frame (initialSkip) (applied before stream on)
         * The 2nd result will take effect
         *        @ frame (0+lag) (applied @ SOF 0)
         *     or @ frame (intialSkip+1) if (intialSkip+1 > 0+lag)
         * So for the latter case, the 1st result should be repeated
         * to delay applying SOF of the 2nd result.
         */
        skipNum = PlatformData::getInitialSkipFrame(mCameraId) + 1 - PlatformData::getExposureLag(mCameraId);
        if (skipNum < 0) {
            skipNum = 0;
        }
    }

    return skipNum;
}

bool AiqEngine::needRun3A(AiqStatistics *aiqStatistics)
{
    LOG3A("%s", __func__);

    // Force to run 3a for per-frame control case
    if (mAiqRunningForPerframe) {
        return true;
    }

    // Force to run 3a for the first time running
    if (mFirstAiqRunning) {
        return true;
    }

    if (aiqStatistics == nullptr) {
        LOG3A("no stats and not need to re-run 3A");
        return false;
    }

    if (mLastStatsSequence == aiqStatistics->mSequence) {
        LOG3A("no new stats skip, mLastStatsSequence = %ld", mLastStatsSequence);
        return false;
    } else if (mSensorManager->getCurrentExposureAppliedDelay() > kMaxExposureAppliedDelay) {
        LOG3A("exposure setting applied delay is too larger, skip it");
        return false;
    }

    return true;
}

AiqEngine::AiqState AiqEngine::prepareInputParam(void)
{
    // set Aiq Params
    int ret = mAiqSetting->getAiqParameter(mAiqParam);
    if (ret != OK)
        return AIQ_STATE_ERROR;

    // Update sensor info for the first-run of AIQ
    if (mFirstAiqRunning) {
        mSensorManager->setFrameRate(mAiqParam.fps);
        // set sensor info if needed
        ia_aiq_exposure_sensor_descriptor sensorDescriptor;
        ia_aiq_frame_params frameParams;
        CLEAR(sensorDescriptor);
        CLEAR(frameParams);
        ret = mSensorManager->getSensorInfo(frameParams, sensorDescriptor);
        Check((ret != OK), AIQ_STATE_ERROR, "Get sensor info failed:%d", ret);
        mAiqCore->setSensorInfo(frameParams, sensorDescriptor);
    }

    // update lens related parameters
    mLensManager->getLensInfo(mAiqParam);

    mAiqCore->updateParameter(mAiqParam);
    // set Stats
    ia_aiq_statistics_input_params_v4 statsParam;
    CLEAR(statsParam);
    ia_aiq_gbce_results gbceResults;
    CLEAR(gbceResults);

    AiqStatistics *aiqStats =
        const_cast<AiqStatistics*>(mAiqResultStorage->getAndLockAiqStatistics());

    if (!needRun3A(aiqStats)) {
        return AIQ_STATE_WAIT;
    }

    if (aiqStats == nullptr) {
        LOG3A("%s: run aiq without stats data", __func__);
        return AIQ_STATE_RUN;
    }

    ret = prepareStats(&statsParam, &gbceResults, aiqStats);

    if (ret != OK) {
        LOG3A("%s: no useful stats", __func__);
        return AIQ_STATE_RUN;
    }

    mAiqCore->setStatistics(&statsParam);

    mAiqCore->setGbceResults(&gbceResults);

    // LOCAL_TONEMAP_S
    if ((mAiqParam.tuningMode == TUNING_MODE_VIDEO_HDR) || (mAiqParam.tuningMode == TUNING_MODE_VIDEO_HDR2) ) {
        ltm_result_t *ltmResult = const_cast<ltm_result_t*>(mAiqResultStorage->getLtmResult());
        if (ltmResult == nullptr) {
            LOG3A("%s: no ltm results", __func__);
            return AIQ_STATE_RUN;
        }
        LOG3A("%s: ltm results sequence %ld", __func__, ltmResult->sequence);

        ia_isp_bxt_hdr_yv_grid_t hdrYvGrid;
        CLEAR(hdrYvGrid);
        hdrYvGrid.grid_width = ltmResult->yvGridInfo.width;
        hdrYvGrid.grid_height = ltmResult->yvGridInfo.height;

        ia_ltm_input_params ltmInputParams;
        CLEAR(ltmInputParams);
        ltmInputParams.yv_grid = &hdrYvGrid;

        mAiqCore->setLtmParams(&ltmInputParams, &ltmResult->ltmDrcParams);
    }
    // LOCAL_TONEMAP_E

    return AIQ_STATE_RUN;
}

AiqEngine::AiqState AiqEngine::runAiq(AiqResult *aiqResult)
{
    int ret = mAiqCore->runAiq(aiqResult);
    if (ret != OK) {
        return AIQ_STATE_ERROR;
    }

    return AIQ_STATE_RESULT_SET;
}

AiqEngine::AiqState AiqEngine::handleAiqResult(AiqResult *aiqResult)
{
    LOG2("%s: aiqResult->mTuningMode = %d", __func__, aiqResult->mTuningMode);

    aec_scene_t aecScene = (aiqResult->mAeResults.multiframe == ia_aiq_bracket_mode_ull) ?
                            AEC_SCENE_ULL : AEC_SCENE_HDR;
    mAiqSetting->updateTuningMode(aecScene);

    return AIQ_STATE_DONE;
}

AiqEngine::AiqState AiqEngine::done(AiqResult *aiqResult)
{
    int skipNum = getSkippingNum(aiqResult);
    AiqResult *tmp = aiqResult;

    for (int i = 0; i < skipNum; i++) {
        // Increase the sensor settings sequence id, so for any frame that
        // its sequence id is bigger than the user expected id will be discarded.
        setAiqResult(tmp, true);
        mAiqResultStorage->updateAiqResult(tmp->mSequence);
        tmp = mAiqResultStorage->acquireAiqResult();
        *tmp = *aiqResult;
    }

    setAiqResult(tmp, false);
    mAiqResultStorage->updateAiqResult(tmp->mSequence);

    mFirstAiqRunning = false;
    return AIQ_STATE_WAIT;
}

} /* namespace icamera */
