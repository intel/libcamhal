/*
 * Copyright (C) 2016-2018 Intel Corporation.
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

#define LOG_TAG "CASE_PAL_P2P"

#include <vector>
#include <time.h>

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include <linux/intel-ipu4-isys.h>

#include "PlatformData.h"
#include "CameraBuffer.h"
#include "IspParamAdaptor.h"
#include "AiqResultStorage.h"
#include "MockSysCall.h"
#include "case_common.h"

using namespace icamera;
using std::vector;

/**
 * Sub class of CameraBuffer, it's used for unit test, as CameraBuffer only used for mmap currently,
 * but in this unit test, we want to malloc buffers directly.
 */
class TestBuffer : public CameraBuffer
{
public:
    /**
     * Use invalid parameter to construct its parent class, and update members later, this will cause an error
     * printed in the constructor of CameraBuffer, but it's no harm, please just ignore it.
     */
    TestBuffer() : CameraBuffer(0, BUFFER_USAGE_ISA_PARAM,
                                V4L2_MEMORY_USERPTR, 0, 0,
                                V4L2_FMT_INTEL_IPU4_ISA_CFG) {}
    ~TestBuffer() {
        deallocate();
    }

    int allocBuffer(int plane[], int len) {
        for (int i=0; i<len; i++) {
            void *addr = malloc(plane[i]);
            setBufferSize(plane[i], i);
            setBufferAddr(addr, i);
        }
        if (getBufferAddr(0) == NULL || getBufferAddr(1) == NULL) {
            deallocate();
            return NO_MEMORY;
        }

        return OK;
    }

    void deallocate() {
        for (int i = 0; i < numPlanes(); i++) {
            void *addr = getBufferAddr(i);
            if (addr != NULL) {
                free(addr);
                setBufferAddr(NULL, 0);
            }
        }
    }
};

class camPalP2pTest: public testing::Test {
protected:
    virtual void SetUp() {
        int cameraNum = PlatformData::numberOfCameras();
        for (int cameraId = 0; cameraId < cameraNum; cameraId++) {
            if (PlatformData::isEnableAIQ(cameraId)) {
                if (strcmp(PlatformData::getSensorName(cameraId), "imx185") == 0) {
                    mCameras.push_back(cameraId);
                }
            }
        }

        mConfigMode = CAMERA_STREAM_CONFIGURATION_MODE_NORMAL;
        mTuningMode = TUNING_MODE_VIDEO;
        CLEAR(mStream);
        mStream.width = 1920;
        mStream.height = 1080;
        mStream.format = V4L2_PIX_FMT_NV12;
    }

    virtual void TearDown() {
        mCameras.clear();
        PlatformData::releaseInstance();
    }

    void gcConfigStreams(int cameraId) {
        stream_config_t streamList;
        stream_t streams[1];
        streams[0] = mStream;
        streamList.num_streams = 1;
        streamList.streams = streams;
        streamList.operation_mode = mConfigMode;
        IGraphConfigManager::getInstance(cameraId)->configStreams(&streamList);
    }

    int preparePalP2pEncodeIsaParam(int cameraId, IspParamAdaptor* isaAdaptor,
                                    std::shared_ptr<TestBuffer> pbuf)
    {
        isaAdaptor->init();
        isaAdaptor->configure(mStream, mConfigMode, mTuningMode);

        int inputSize = isaAdaptor->getInputPayloadSize();
        int pgSize = isaAdaptor->getProcessGroupSize();

        // Buffer size should be got from driver, here we just use the size from PAL/P2P library,
        // and add 1024 as additional buffer.
        int plane[2];
        plane[0]= pgSize + 1024;
        plane[1] = inputSize + 1024;
        int ret = pbuf->allocBuffer(plane, 2);
        return ret;
    }

    vector<int> mCameras;
    ConfigMode mConfigMode;
    TuningMode mTuningMode;
    stream_t mStream;
};

/**
 * Test if isp adaptor can be initialized
 */
TEST_F(camPalP2pTest, pal_p2p_init) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];
        IspParamAdaptor isaAdaptor(cameraId, PG_PARAM_ISYS);
        int ret = isaAdaptor.init();
        EXPECT_EQ(OK, ret);

        ret = isaAdaptor.deinit();
        EXPECT_EQ(OK, ret);

        IspParamAdaptor psysAdaptor(cameraId, PG_PARAM_PSYS_ISA);
        ret = psysAdaptor.init();
        EXPECT_EQ(OK, ret);

        ret = psysAdaptor.deinit();
        EXPECT_EQ(OK, ret);
    }
}

/**
 * Test if isp adaptor can be configured for isa
 */
TEST_F(camPalP2pTest, pal_p2p_configure_isa) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        IspParamAdaptor isaAdaptor(cameraId, PG_PARAM_ISYS);
        int ret = isaAdaptor.init();
        EXPECT_EQ(OK, ret);

        ret = isaAdaptor.configure(mStream, mConfigMode, mTuningMode);
        EXPECT_EQ(OK, ret);

        int inputSize = isaAdaptor.getInputPayloadSize();
        int outputSize = isaAdaptor.getOutputPayloadSize();
        int pgSize = isaAdaptor.getProcessGroupSize();
        EXPECT_GT(inputSize, 0);
        EXPECT_GT(outputSize, 0);
        EXPECT_GT(pgSize, 0);

        ret = isaAdaptor.deinit();
        EXPECT_EQ(OK, ret);
    }
}

/**
 * Test if isa parameters can be encoded with empty aiq results
 */
TEST_F(camPalP2pTest, pal_p2p_encode_isa_param_with_empty_aiqresult) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        IspParamAdaptor isaAdaptor(cameraId, PG_PARAM_ISYS);
        std::shared_ptr<TestBuffer> buffer = std::make_shared<TestBuffer>();
        Check(!buffer, VOID_VALUE, "@%s: fail to alloc CameraBuffer", __func__);
        int ret = preparePalP2pEncodeIsaParam(cameraId, &isaAdaptor, buffer);
        if (ret != OK) {
            // Don't fail the case when buffer cannot be allocated, since this is
            // not the case is used for.
            LOGW("Failed to alloc buffer for encode isa, skipping test...");
            isaAdaptor.deinit();
            return;
        }

        ret = isaAdaptor.encodeIsaParams(buffer, ENCODE_ISA_CONFIG);
        EXPECT_EQ(OK, ret);

        ret = isaAdaptor.deinit();
        EXPECT_EQ(OK, ret);
    }
}

/**
 * Test if isa parameters can be encoded with normal aiq results
 */
TEST_F(camPalP2pTest, pal_p2p_encode_isa_param_with_normal_aiqresult) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        IspParamAdaptor isaAdaptor(cameraId, PG_PARAM_ISYS);
        std::shared_ptr<TestBuffer> buffer = std::make_shared<TestBuffer>();
        Check(!buffer, VOID_VALUE, "@%s: fail to alloc CameraBuffer", __func__);
        int ret = preparePalP2pEncodeIsaParam(cameraId, &isaAdaptor, buffer);
        if (ret != OK) {
            LOGW("Failed to alloc buffer for encode isa, skipping test...");
            isaAdaptor.deinit();
            return;
        }

        AiqResult *aiqResult = AiqResultStorage::getInstance(cameraId)->acquireAiqResult();

        // Fill aiq result with random values
        aiqResult->mSaResults.lsc_update = 1;

        aiqResult->mAeResults.exposures->converged = 1;
        aiqResult->mAeResults.exposures->exposure->exposure_time_us = 10 * 1000 * 1000;
        aiqResult->mAeResults.exposures->exposure->analog_gain = 50;
        aiqResult->mAeResults.exposures->exposure->iso = 400;

        AiqResultStorage::getInstance(cameraId)->updateAiqResult(0);

        ret = isaAdaptor.encodeIsaParams(buffer, ENCODE_STATS);
        EXPECT_EQ(OK, ret);

        ret = isaAdaptor.deinit();
        EXPECT_EQ(OK, ret);

        AiqResultStorage::releaseAiqResultStorage(cameraId);
    }
}

/**
 * Test if aiq stats can be encoded
 */
TEST_F(camPalP2pTest, pal_p2p_decode_aiq_stats) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        IspParamAdaptor isaAdaptor(cameraId, PG_PARAM_ISYS);
        isaAdaptor.init();
        isaAdaptor.configure(mStream, mConfigMode, mTuningMode);

        int outputSize = isaAdaptor.getOutputPayloadSize();
        int pgSize = isaAdaptor.getProcessGroupSize();

        // Buffer size should be got from driver, here we just use the size from PAL/P2P library,
        // and add 1024 as additional buffer.
        int plane[2];
        plane[0]= pgSize + 1024;
        plane[1] = outputSize + 1024;
        std::shared_ptr<TestBuffer> buffer = std::make_shared<TestBuffer>();
        Check(!buffer, VOID_VALUE, "@%s: fail to alloc CameraBuffer", __func__);
        int ret = buffer->allocBuffer(plane, 2);
        if (ret != OK) {
            LOGW("Failed to alloc buffer for encode isa, skipping test...");
            isaAdaptor.deinit();
            return;
        }

        isaAdaptor.encodeIsaParams(buffer, ENCODE_STATS);

        ia_aiq_rgbs_grid* rgbs_grid = NULL;
        ia_aiq_af_grid* af_grid = NULL;
        ret =  isaAdaptor.convertIsaRgbsStatistics(buffer, &rgbs_grid);
        ret |= isaAdaptor.convertIsaAfStatistics(buffer, &af_grid);
        EXPECT_EQ(OK, ret);
        EXPECT_NOT_NULL(rgbs_grid);
        EXPECT_NOT_NULL(af_grid);
        if (rgbs_grid == NULL || af_grid == NULL) {
            isaAdaptor.deinit();
            return;
        }

        LOGD("rgbs grid w x h: %dx%d", rgbs_grid->grid_width, rgbs_grid->grid_height);
        LOGD("af grid w x h: %dx%d", af_grid->grid_width, af_grid->grid_height);

        EXPECT_NOT_NULL(rgbs_grid->blocks_ptr);
        EXPECT_TRUE(rgbs_grid->grid_width > 0 && rgbs_grid->grid_width < 128);
        EXPECT_TRUE(rgbs_grid->grid_height> 0 && rgbs_grid->grid_height < 128);
        EXPECT_TRUE(af_grid->grid_width > 0 && af_grid->grid_width < 128);
        EXPECT_TRUE(af_grid->grid_height> 0 && af_grid->grid_height < 128);

        ret = isaAdaptor.deinit();
        EXPECT_EQ(OK, ret);
    }
}

/**
 * Test the performance of isa parameters encoding and stats converting
 */
TEST_F(camPalP2pTest, pal_p2p_performance_isa) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        clock_t beforeInit;
        clock_t beforeConfigure;
        clock_t afterConfigure;
        clock_t beforeEncode;
        clock_t afterEncode;
        clock_t afterConvert;

        beforeInit = clock();
        IspParamAdaptor isaAdaptor(cameraId, PG_PARAM_ISYS);
        isaAdaptor.init();

        beforeConfigure = clock();
        isaAdaptor.configure(mStream, mConfigMode, mTuningMode);
        afterConfigure = clock();

        int inputSize = isaAdaptor.getInputPayloadSize();
        int pgSize = isaAdaptor.getProcessGroupSize();

        // Buffer size should be got from driver, here we just use the size from PAL/P2P library,
        // and add 1024 as additional buffer.
        int plane[2];
        plane[0]= pgSize + 1024;
        plane[1] = inputSize + 1024;
        std::shared_ptr<TestBuffer> buffer = std::make_shared<TestBuffer>();
        Check(!buffer, VOID_VALUE, "@%s: fail to alloc CameraBuffer", __func__);
        int ret = buffer->allocBuffer(plane, 2);
        if (ret != OK) {
            LOGW("Failed to alloc buffer for encode isa, skipping test...");
            isaAdaptor.deinit();
            return;
        }

        beforeEncode = clock();
        isaAdaptor.encodeIsaParams(buffer, ENCODE_STATS);
        afterEncode = clock();

        ia_aiq_rgbs_grid* rgbs_grid = NULL;
        ia_aiq_af_grid* af_grid = NULL;
        ret =  isaAdaptor.convertIsaRgbsStatistics(buffer, &rgbs_grid);
        ret |= isaAdaptor.convertIsaAfStatistics(buffer, &af_grid);
        afterConvert = clock();

        float initTime = (beforeConfigure - beforeInit) * 1000 / CLOCKS_PER_SEC;
        float configureTime = (afterConfigure - beforeConfigure) * 1000 / CLOCKS_PER_SEC;
        float encodeTime = (afterEncode - beforeEncode) * 1000 / CLOCKS_PER_SEC;
        float convertTime = (afterConvert - afterEncode) * 1000 / CLOCKS_PER_SEC;
        LOGD("init time:%fms, configure time:%fms, encode time:%fms convert time:%fms",
                initTime, configureTime, encodeTime, convertTime);

        EXPECT_LT(initTime, 200);
        EXPECT_LT(configureTime, 100);
        // Encoding and converting happnens every frame, so it must not be too long
        EXPECT_LT(encodeTime, 10);
        EXPECT_LT(convertTime, 5);

        isaAdaptor.deinit();
    }
}

/**
 * Test if isp adaptor can be configured for psys
 */
TEST_F(camPalP2pTest, pal_p2p_configure_psys) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        IspParamAdaptor pSysAdaptor(cameraId, PG_PARAM_PSYS_ISA);
        int ret = pSysAdaptor.init();
        EXPECT_EQ(OK, ret);

        ret = pSysAdaptor.configure(mStream, mConfigMode, mTuningMode);
        EXPECT_EQ(OK, ret);

        ret = pSysAdaptor.deinit();
        EXPECT_EQ(OK, ret);
    }
}

/**
 * Test if isp adaptor can be configured for psys
 */
TEST_F(camPalP2pTest, pal_p2p_psys_run_isp) {
    for (int i = 0; i < mCameras.size(); i++) {
        int cameraId = mCameras[i];

        gcConfigStreams(cameraId);

        IspParamAdaptor pSysAdaptor(cameraId, PG_PARAM_PSYS_ISA);
        int ret = pSysAdaptor.init();
        ret = pSysAdaptor.configure(mStream, mConfigMode, mTuningMode);
        const ia_binary_data* ia_data = pSysAdaptor.getIpuParameter();
        EXPECT_NOT_NULL(ia_data);
        EXPECT_NOT_NULL(ia_data->data);
        EXPECT_GT(ia_data->size, 0);

        ret = pSysAdaptor.runIspAdapt(NULL);
        EXPECT_EQ(OK, ret);

        ia_data = pSysAdaptor.getIpuParameter();
        EXPECT_NOT_NULL(ia_data);
        EXPECT_NOT_NULL(ia_data->data);
        EXPECT_GT(ia_data->size, 0);

        ret = pSysAdaptor.deinit();
        EXPECT_EQ(OK, ret);
    }
}

