/*
 * Copyright (C) 2017-2018 Intel Corporation.
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

#define LOG_TAG "Ltm"

#include <cmath>

#include "Ltm.h"

#include "iutils/Utils.h"
#include "iutils/Errors.h"
#include "iutils/CameraLog.h"

#include "IntelMkn.h"
#include "PlatformData.h"
#include "AiqResultStorage.h"
#include "AiqUtils.h"

#include "ia_log.h"

namespace icamera {

Ltm::Ltm(int cameraId) :
    mCameraId(cameraId),
    mLtm(nullptr),
    mTuningMode(TUNING_MODE_MAX),
    mLtmState(LTM_NOT_INIT),
    mThreadRunning(false),
    mInputParamIndex(-1)
{
    CLEAR(mLtmParams);
    CLEAR(mSisBuffer);
    CLEAR(mLtmBinParam);
    mLtmThread = new LtmThread(this);
    LOG3A("%s", __func__);
}

Ltm::~Ltm()
{
    LOG3A("%s", __func__);
    mLtmThread->join();
    delete mLtmThread;
}

int Ltm::initLtmParams()
{
    for (int i = 0; i < kMaxLtmParamsNum; i++) {
        mLtmParams[i]->ltmParams.ltm_level = ia_ltm_level_use_tuning;
        mLtmParams[i]->ltmParams.frame_use = ia_aiq_frame_use_video;
        mLtmParams[i]->ltmParams.ev_shift = 0;
        mLtmParams[i]->ltmParams.ltm_strength_manual = 100;
        mLtmParams[i]->ltmParams.gtm_input_params_ptr = &(mLtmParams[i]->gtmParams);

        mLtmParams[i]->gtmParams.manual_convergence_time = -1;
        mLtmParams[i]->gtmParams.manual_gain = -1;
        mLtmParams[i]->gtmParams.frame_timestamp = 0;
    }

    return OK;
}

int Ltm::init()
{
    LOG3A("%s", __func__);

    if (!PlatformData::isEnableHDR(mCameraId)) {
        return OK;
    }

    AutoMutex l(mLtmLock);

    for (int i = 0; i < kMaxLtmParamsNum; i++) {
        mLtmParams[i] = new LtmInputParams;
        mSisBuffer[i] = new SisBuffer;
    }

    initLtmParams();

    mLtmState = LTM_INIT;

    return OK;
}

int Ltm::deinit()
{
    LOG3A("%s", __func__);

    if (!PlatformData::isEnableHDR(mCameraId)) {
        return OK;
    }

    deinitIaLtmHandle();

    AutoMutex l(mLtmLock);

    for (int i = 0; i < kMaxLtmParamsNum; i++) {
        if (mSisBuffer[i]->sisImage.data) {
            free(mSisBuffer[i]->sisImage.data);
            mSisBuffer[i]->sisImage.data = nullptr;
        }

        if (mLtmParams[i]->ltmParams.input_image_ptr) {
            free(mLtmParams[i]->ltmParams.input_image_ptr);
            mLtmParams[i]->ltmParams.input_image_ptr = nullptr;
        }

        delete mSisBuffer[i];
        mSisBuffer[i] = nullptr;

        delete mLtmParams[i];
        mLtmParams[i] = nullptr;
    }
    mLtmState = LTM_NOT_INIT;

    return OK;
}

int Ltm::initIaLtmHandle(TuningMode tuningMode)
{
    LOG3A("%s", __func__);

    ia_mkn *mkn = IntelMkn::getInstance(mCameraId)->getMknHandle();
    Check((mkn == nullptr), NO_INIT, "Error in initing makernote");

    CpfStore* cpf = PlatformData::getCpfStore(mCameraId);
    Check((cpf == nullptr), NO_INIT, "@%s, No CPF for cameraId:%d", __func__, mCameraId);

    ia_binary_data otherData;
    ia_cmc_t *cmc;

    int ret = cpf->getDataAndCmc(nullptr, nullptr, &otherData, &cmc, tuningMode);
    Check(ret != OK, BAD_VALUE, "@%s, Get cpf data failed", __func__);
    {
        PERF_CAMERA_ATRACE_PARAM1("ia_ltm_init", 0);
        mLtm = ia_ltm_init((ia_binary_data*)&(otherData), mkn);
    }
    Check(mLtm == nullptr, NO_INIT, "Failed to init ltm");

    return OK;
}

int Ltm::deinitIaLtmHandle()
{
    LOG3A("%s", __func__);

    if (mLtm) {
        PERF_CAMERA_ATRACE_PARAM1("ia_ltm_deinit", 0);
        ia_ltm_deinit(mLtm);
        mLtm = nullptr;
    }

    return OK;
}

int Ltm::configure(const vector<ConfigMode>& configModes)
{
    LOG3A("%s", __func__);

    if (!PlatformData::isEnableHDR(mCameraId)) {
        return OK;
    }

    TuningMode tMode = TUNING_MODE_MAX;
    for (auto cfg : configModes) {
        // Only support the 1st tuning mode if multiple config mode is configured.
        if (cfg == CAMERA_STREAM_CONFIGURATION_MODE_HLC) {
            tMode = TUNING_MODE_VIDEO_HLC;
            break;
        } else if (cfg == CAMERA_STREAM_CONFIGURATION_MODE_HDR) {
            tMode = TUNING_MODE_VIDEO_HDR;
            break;
        } else if (cfg == CAMERA_STREAM_CONFIGURATION_MODE_HDR2) {
            tMode = TUNING_MODE_VIDEO_HDR2;
            break;
        }else if (cfg == CAMERA_STREAM_CONFIGURATION_MODE_NORMAL) {
            tMode = TUNING_MODE_VIDEO;
            break;
        }
    }

    if (tMode == TUNING_MODE_MAX) {
        return OK;
    }

    if (mLtmState == LTM_CONFIGURED && mTuningMode == tMode) {
        return OK;
    }

    deinitIaLtmHandle();

    int ret = initIaLtmHandle(tMode);
    Check((ret != OK), ret, "%s, configure LTM algo failed %d", __func__, ret);

    mTuningMode = tMode;
    mLtmState = LTM_CONFIGURED;

    updateTuningData();

    LOG3A("%s Ltm algo is Configured", __func__);
    return OK;
}

int Ltm::start()
{
    LOG1("@%s", __func__);
    AutoMutex l(mLtmLock);

    if (PlatformData::isEnableLtmThread(mCameraId)) {
        mThreadRunning = true;
        mLtmThread->run("ltm_thread", PRIORITY_NORMAL);
    }

    return OK;
}

void Ltm::stop()
{
    LOG1("@%s", __func__);

    if (PlatformData::isEnableLtmThread(mCameraId)) {
        mLtmThread->requestExit();
        {
            AutoMutex l(mLtmLock);
            mThreadRunning = false;
            mParamAvailableSignal.signal();
        }
        mLtmThread->requestExitAndWait();
    }

    while (!mLtmParamsQ.empty()) {
        mLtmParamsQ.pop();
    }
}

void Ltm::handleEvent(EventData eventData)
{
    if (eventData.type == EVENT_PSYS_STATS_BUF_READY) {
        LOG3A("%s: handle EVENT_PSYS_STATS_BUF_READY", __func__);
        long sequence = eventData.data.statsReady.sequence;
        unsigned long long timestamp = TIMEVAL2USECS(eventData.data.statsReady.timestamp);

        LtmStatistics *ltmStatistics = AiqResultStorage::getInstance(mCameraId)->getLtmStatistics();
        if (ltmStatistics->sequence != sequence || ltmStatistics->hdrYvGrid == nullptr) return;

        handleLtm(ltmStatistics->hdrYvGrid, timestamp, sequence);
    } else if (eventData.type == EVENT_PSYS_STATS_SIS_BUF_READY) {
        LOG3A("%s: handle EVENT_PSYS_STATS_SIS_BUF_READY", __func__);
        handleSisLtm(eventData.buffer);
    }
}

AiqResult *Ltm::getAiqResult(long sequence)
{
    long ltmSequence = sequence;
    AiqResultStorage *resultStorage = AiqResultStorage::getInstance(mCameraId);
    if (ltmSequence > 0) {
        ltmSequence += PlatformData::getLtmGainLag(mCameraId);
    }

    LOG3A("%s, ltmSequence %ld, sequence %ld", __func__, ltmSequence, sequence);
    AiqResult* feedback = const_cast<AiqResult*>(resultStorage->getAiqResult(ltmSequence));
    if (feedback == nullptr) {
        LOGW("%s: no feed back result for sequence %ld! use the latest instead",
                __func__, ltmSequence);
        feedback = const_cast<AiqResult*>(resultStorage->getAiqResult());
    }

    return feedback;
}

int Ltm::handleLtm(ia_isp_bxt_hdr_yv_grid_t* hdrYvGrid, unsigned long long timestamp, long sequence)
{
    LOG3A("@%s", __func__);
    if (!PlatformData::isEnableHDR(mCameraId)) {
        return OK;
    }
    AutoMutex l(mLtmLock);

    mInputParamIndex++;
    mInputParamIndex %= kMaxLtmParamsNum;

    if (hdrYvGrid) {
        mLtmParams[mInputParamIndex]->hdrYvGrid = *hdrYvGrid;
        mLtmParams[mInputParamIndex]->ltmParams.yv_grid = &(mLtmParams[mInputParamIndex]->hdrYvGrid);
    } else {
        mLtmParams[mInputParamIndex]->ltmParams.yv_grid = nullptr;
    }

    AiqResult* feedback = getAiqResult(sequence);
    updateParameter(feedback->mAiqParam, timestamp);

    if ((!PlatformData::isEnableLtmThread(mCameraId)) || sequence == 0) {
        AiqResultStorage *resultStorage = AiqResultStorage::getInstance(mCameraId);
        ltm_result_t *ltmResult = resultStorage->acquireLtmResult();

        runLtm(&(feedback->mAeResults), ltmResult);
        resultStorage->updateLtmResult(sequence);
        updateTuningData();
    } else {
        mLtmParams[mInputParamIndex]->sequence = sequence;
        bool needSignal = mLtmParamsQ.empty();
        mLtmParamsQ.push(mLtmParams[mInputParamIndex]);
        if (needSignal) {
            mParamAvailableSignal.signal();
        }
    }

    return OK;
}

int Ltm::handleSisLtm(const shared_ptr<CameraBuffer> &cameraBuffer)
{
    LOG3A("@%s", __func__);
    if (!PlatformData::isEnableHDR(mCameraId)) {
        return OK;
    }

    AutoMutex l(mLtmLock);

    ia_binary_data* sisFrame = (ia_binary_data*)cameraBuffer->getBufferAddr();
    int sisWidth = cameraBuffer->getWidth();
    int sisHeight = cameraBuffer->getHeight();
    int sequence = cameraBuffer->getSequence();

    mInputParamIndex++;
    mInputParamIndex %= kMaxLtmParamsNum;

    AiqResult *feedback = getAiqResult(sequence);

    int size = sisFrame->size;
    Check((size <= 0), BAD_VALUE, "sis data size err!");

    void *data = sisFrame->data;
    Check((data == nullptr), BAD_VALUE, "sis data ptr err!");

    ia_ltm_input_image *inputImagePtr = mLtmParams[mInputParamIndex]->ltmParams.input_image_ptr;
    if (!inputImagePtr) {
        inputImagePtr = (ia_ltm_input_image *)malloc(sizeof(ia_ltm_input_image));
        Check((inputImagePtr == nullptr), NO_INIT, "Error in initing image ptr");

        memset(inputImagePtr, 0, sizeof(ia_ltm_input_image));
        mLtmParams[mInputParamIndex]->ltmParams.input_image_ptr = inputImagePtr;

        inputImagePtr->image_info.raw_image.data_format = ia_image_data_format_rawplain16_interleaved;
        inputImagePtr->image_info.raw_image.bayer_order = cmc_bayer_order_grbg;
        inputImagePtr->image_info.raw_image.data_format_bpp = 16;
        inputImagePtr->image_info.raw_image.data_bpp = 12;

        mSisBuffer[mInputParamIndex]->sisPort = SIS_A;
        mSisBuffer[mInputParamIndex]->sisImage.size = size;
        mSisBuffer[mInputParamIndex]->sisImage.data = malloc(size);
        Check((mSisBuffer[mInputParamIndex]->sisImage.data == nullptr),
            NO_MEMORY, "sis buffer allocated failed!");

        MEMCPY_S(mSisBuffer[mInputParamIndex]->sisImage.data, size, data, size);

        inputImagePtr->image_data = &mSisBuffer[mInputParamIndex]->sisImage;
        // width_cols and height_lines are quad count, need to divide 2 for them.
        inputImagePtr->image_info.raw_image.width_cols = sisWidth / 2;
        inputImagePtr->image_info.raw_image.height_lines = sisHeight / 2;

        mLtmBinParam.sParam.gridWidth = sisWidth;
        mLtmBinParam.sParam.gridHeight = sisHeight;
    }

    updateParameter(feedback->mAiqParam, 0);

    if (PlatformData::isEnableLtmThread(mCameraId)) {
        mLtmParams[mInputParamIndex]->sequence = sequence;
        bool needSignal = mLtmParamsQ.empty();

        mLtmParamsQ.push(mLtmParams[mInputParamIndex]);
        if (needSignal) {
            mParamAvailableSignal.signal();
        }
    }

    return OK;
}

int Ltm::runLtm()
{
    LtmInputParams *inputParams = NULL;

    AiqResultStorage *resultStorage = AiqResultStorage::getInstance(mCameraId);
    ConditionLock lock(mLtmLock);

    while(mLtmParamsQ.empty()) {
        // To prevent possible dead lock during stop of ltm thread.
        if (!mThreadRunning) {
            LOG2("%s, ltm thread is not active, no need to wait ltm stat", __func__);
            return OK;
        }

        mParamAvailableSignal.wait(lock);

        if (!mThreadRunning) {
            LOG2("%s, ltm thread is not active while waiting ltm stat", __func__);
            return OK;
        }
    }

    Check(mLtmParamsQ.empty(), UNKNOWN_ERROR, "Failed to get ltm input params buffers");
    inputParams = mLtmParamsQ.front();
    mLtmParamsQ.pop();
    Check(!inputParams, OK, "%s, the inputParams is NULL", __func__);

    ltm_result_t *ltmResult = resultStorage->acquireLtmResult();
    LOG1("@%s the sequence: %ld", __func__, inputParams->sequence);
    AiqResult *feedback = getAiqResult(inputParams->sequence);

    mLtmBinParam.sequence = inputParams->sequence;
    runLtm(&(feedback->mAeResults), ltmResult, &(inputParams->ltmParams));
    resultStorage->updateLtmResult(inputParams->sequence);

    updateTuningData();

    return OK;
}

int Ltm::runLtm(ia_aiq_ae_results* aeResult, ltm_result_t *ltmResult, ia_ltm_input_params *ltmParams)
{
    LOG3A("%s", __func__);
    PERF_CAMERA_ATRACE();

    if (!PlatformData::isEnableHDR(mCameraId)
        || mLtmState != LTM_CONFIGURED) {
        return OK;
    }

    ia_ltm_results *tmpLtmResults = nullptr;
    ia_ltm_drc_params *tmpLtmDrcParams = nullptr;
    ia_ltm_input_params *tmpLtmParams = ltmParams;

    if (tmpLtmParams == NULL) {
        tmpLtmParams = &(mLtmParams[mInputParamIndex]->ltmParams);
    }

    if (!tmpLtmParams->yv_grid) {
        // ltm can run without Yv grid and default ltm params will be used.
        LOGD("mHdrYvGrid is Null.");
    }
    tmpLtmParams->ae_results = aeResult;

    LOG3A("%s: begin running LTM", __func__);
    ia_err iaErr;
    {
        PERF_CAMERA_ATRACE_PARAM1_IMAGING("ia_ltm_run", 0);
        iaErr = ia_ltm_run(mLtm, tmpLtmParams, &tmpLtmResults, &tmpLtmDrcParams);
    }

    int ret = AiqUtils::convertError(iaErr);
    Check(ret != OK, ret, "Error running LTM: %d", ret);

    LOG3A("%s: LTM GAIN = %lf", __func__, tmpLtmResults->ltm_gain);

    dumpLtmDrcParams(tmpLtmDrcParams);
    dumpLtmResultsParams(tmpLtmResults);

    {
        PERF_CAMERA_ATRACE_PARAM1("deepCopyLtmResults", 0);
        ret = AiqUtils::deepCopyLtmResults(*tmpLtmResults, &ltmResult->ltmResults);
    }
    Check(ret != OK, ret, "Error on copying LTM results: %d", ret);

    {
        PERF_CAMERA_ATRACE_PARAM1("deepCopyLtmDRCParams", 0);
        ret = AiqUtils::deepCopyLtmDRCParams(*tmpLtmDrcParams, &ltmResult->ltmDrcParams);
    }
    Check(ret != OK, ret, "Error on copying DRC results: %d", ret);

    ltmResult->yvGridInfo.width = tmpLtmParams->yv_grid ? tmpLtmParams->yv_grid->grid_width : 0;
    ltmResult->yvGridInfo.height = tmpLtmParams->yv_grid ? tmpLtmParams->yv_grid->grid_height : 0;

    return OK;
}

int Ltm::updateTuningData()
{
    LOG3A("%s", __func__);

    if (!PlatformData::isEnableHDR(mCameraId) || mLtmState != LTM_CONFIGURED) {
        return INVALID_OPERATION;
    }

    AiqResultStorage *resultStorage = AiqResultStorage::getInstance(mCameraId);
    ltm_tuning_data *tuningData = resultStorage->acquireLtmTuningData();

    Check(mLtm == nullptr, INVALID_OPERATION, "LTM not initialized yet.");
    Check(tuningData == nullptr, BAD_VALUE, "Invalid tuning data storage.");

    char* pLtmTuningData = ((char*)mLtm) + kLtmTuningDataOffset;
    ltm_tuning_data *tuning = (ltm_tuning_data *)pLtmTuningData;

    if(tuning->algo_mode == ltm_algo_tme) {
        /* Overwrite tuning parameter from sensor config (xml).
           This would help switching modes without switching aiqb files */
        int enable_defog = PlatformData::isEnableDefog(mCameraId);
        tuning->defog_tuning.defog_activaton = enable_defog;
    }
    MEMCPY_S(tuningData, sizeof(ltm_tuning_data),
             pLtmTuningData, sizeof(ltm_tuning_data));

    resultStorage->updateLtmTuningData();

    return OK;
}

int Ltm::updateParameter(const aiq_parameter_t &param, unsigned long long timestamp)
{
    LOG3A("%s: frame resolution %dx%d", __func__, param.resolution.width, param.resolution.height);

    mLtmParams[mInputParamIndex]->ltmParams.ev_shift = param.evShift;
    mLtmParams[mInputParamIndex]->ltmParams.ltm_strength_manual = param.hdrLevel;
    mLtmParams[mInputParamIndex]->ltmParams.frame_width = param.resolution.width;
    mLtmParams[mInputParamIndex]->ltmParams.frame_height = param.resolution.height;

    if (param.aeConvergeSpeedMode == CONVERGE_SPEED_MODE_AIQ) {
        mLtmParams[mInputParamIndex]->gtmParams.manual_convergence_time =
            AiqUtils::convertSpeedModeToTimeForHDR(param.aeConvergeSpeed);
    } else {
        mLtmParams[mInputParamIndex]->gtmParams.manual_convergence_time = -1;
    }
    if (param.manualGain >= 0) {
        mLtmParams[mInputParamIndex]->gtmParams.manual_gain = pow(10, (param.manualGain / 20));
    } else {
        mLtmParams[mInputParamIndex]->gtmParams.manual_gain = -1;
    }
    mLtmParams[mInputParamIndex]->gtmParams.frame_timestamp = timestamp;

    LOG3A("%s: Ltm EV shift %f strength %d", __func__,
            mLtmParams[mInputParamIndex]->ltmParams.ev_shift,
            mLtmParams[mInputParamIndex]->ltmParams.ltm_strength_manual);
    LOG3A("%s: Gtm manual gain %f, manual convergence time %f, frame timestamp %llu",
            __func__, mLtmParams[mInputParamIndex]->gtmParams.manual_gain,
            mLtmParams[mInputParamIndex]->gtmParams.manual_convergence_time,
            mLtmParams[mInputParamIndex]->gtmParams.frame_timestamp);

    if (mLtm && param.mLtmTuningEnabled) {
        if (param.mLtmTuningData.algo_mode == ltm_algo_optibright_gain_map) {
            ltm_tuning_data *pLtmTuningData = (ltm_tuning_data*)(((char*)mLtm) + kLtmTuningDataOffset);
            if(pLtmTuningData->algo_mode != param.mLtmTuningData.algo_mode)
            {
                LOGE("LTM algo mode change on-the-fly is not supported:%d", param.mLtmTuningData.algo_mode);
                return INVALID_OPERATION;
            }
            MEMCPY_S(pLtmTuningData, sizeof(param.mLtmTuningData),
                     &param.mLtmTuningData, sizeof(param.mLtmTuningData));
            LOG3A("Override LTM tuning data. algo_mode:%d, grid_density:%d GTM_Str:%d, max_isp_gain:%d",
                   param.mLtmTuningData.algo_mode,
                   param.mLtmTuningData.grid_density,
                   param.mLtmTuningData.optibright_tuning.GTM_Str,
                   param.mLtmTuningData.optibright_tuning.max_isp_gain);
        } else {
            LOGW("Not supported LTM algo mode:%d", param.mLtmTuningData.algo_mode);
        }
    }

    return OK;
}

int Ltm::dumpLtmDrcParams(const ia_ltm_drc_params* ltmDrcParams)
{
    if (!Log::isDebugLevelEnable(CAMERA_DEBUG_LOG_AIQ)) return OK;

    LOG3A("%s", __func__);

    if (!ltmDrcParams){
        LOG2("%s: ltmDrcParams is nullptr, and nothing to dump.", __func__);
        return BAD_VALUE;
    }

    //Only dump first 10 values.
    for (unsigned int i = 0; i < 10; i++) {
       LOG3A("   LTM DRC params matrix gain %u weight %u",
            ltmDrcParams->gain_map[i], ltmDrcParams->weight_map[i]);
    }

    return OK;
}

int Ltm::dumpLtmResultsParams(const ia_ltm_results *ltmResults)
{
    if (!CameraDump::isDumpTypeEnable(DUMP_LTM_OUTPUT)) return OK;

    LOG3A("%s", __func__);

    if (!ltmResults){
        LOG2("%s: ltmResults is nullptr, and nothing to dump.", __func__);
        return BAD_VALUE;
    }

    char fileName[MAX_NAME_LEN] = {'\0'};
    snprintf(fileName, (MAX_NAME_LEN-1), "ia_ltm_luts_%ld_w_%d_h_%d.bin",
        mLtmBinParam.sequence, mLtmBinParam.sParam.gridWidth, mLtmBinParam.sParam.gridHeight);

    CameraDump::writeData(ltmResults->ltm_luts, sizeof(ltmResults->ltm_luts), fileName);

    return OK;
}

} /* namespace icamera */
