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

#define LOG_TAG "CASE_PER_FRAME"

#include <cmath>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "ICamera.h"
//#include "MockSysCall.h"
#include "Parameters.h"
#include "case_common.h"

#define MAX_FRAME_NUM  10
#define MAX_STREAM_NUM  2

enum IspParamType {
    PARAM_AWB_GAIN = 0,
    PARAM_AWB_TRANSFORM,
};

class CamPerFrameTest: public testing::Test {
    protected:
        CamPerFrameTest() {
            mCameraId = getCurrentCameraId();
            mBufferCount = MAX_FRAME_NUM;
            mSensorExposures.min = 100.0;
            mSensorExposures.max = 30000.0;

            mSensorGains.min = 0.0;
            mSensorGains.max = 60.0;
        }

        virtual void SetUp() {
            camera_info_t info;
            get_camera_info(mCameraId, info);
            if (!info.capability) {
                return;
            }

            std::vector<camera_ae_exposure_time_range_t> etRanges;
            info.capability->getSupportedAeExposureTimeRange(etRanges);
            for (unsigned int i = 0; i < etRanges.size(); i++) {
                if (etRanges[i].scene_mode == SCENE_MODE_AUTO || i == 0) {
                    mSensorExposures = etRanges[i].et_range;
                }
            }
            std::vector<camera_ae_gain_range_t> gainRanges;
            info.capability->getSupportedAeGainRange(gainRanges);
            for (unsigned int i = 0; i < gainRanges.size(); i++) {
                if (gainRanges[i].scene_mode == SCENE_MODE_AUTO || i == 0) {
                    mSensorGains = gainRanges[i].gain_range;
                }
            }

        }

        virtual void TearDown() {
        }

        int buildSensorSettings(uint32_t paramsNum, int frameInterval = 1) {
            if (!isFeatureSupported(MANUAL_EXPOSURE)) return 0;

            if (paramsNum < 2) {
                LOGE("params num < 0!");
                return 0;
            }
            if (frameInterval < 1) {
                frameInterval = 1;
            }

            // Use intermediate data of range
            const float exposureStart = mSensorExposures.min + (mSensorExposures.max - mSensorExposures.min) / 4;
            const float exposureEnd = mSensorExposures.max - (mSensorExposures.max - mSensorExposures.min) / 4;
            const float exposureStep = (exposureEnd - exposureStart) / (paramsNum - 1);
            const float gainStart = mSensorGains.min + (mSensorGains.max - mSensorGains.min) / 4;
            const float gainEnd = mSensorGains.max - (mSensorGains.max - mSensorGains.min) / 4;
            const float gainStep = (gainEnd - gainStart) / (paramsNum - 1);

            Parameters settings;
            int i = 0;
            int frameId = 0;
            for (i = 0; i < paramsNum; i ++) {
                frameId = i * frameInterval;
                if (frameId > MAX_FRAME_NUM) {
                    frameId = MAX_FRAME_NUM;
                    break;
                }

                if (mParams.find(frameId) != mParams.end()) {
                    settings = mParams[frameId];
                }
                settings.setAeMode(AE_MODE_MANUAL);

                int64_t exposure = exposureStart + exposureStep * i;
                settings.setExposureTime(exposure);
                float gain = gainStart + gainStep * i;
                settings.setSensitivityGain(gain);
                mParams[frameId] = settings;

                // Print for debug
                LOGD("%s: frame %d, exposure %ld, gain %8.3f(db)", __func__,
                        frameId, exposure, gain);
            }

            return (frameId + 1);
        }

        int checkSensorParams(const Parameters &result, const Parameters *setting = NULL) {
            const float deltaExposure = 0.02; // %
            int64_t settingExposure = 0;
            int64_t resultExposure = 1;
            EXPECT_EQ((result.getExposureTime(resultExposure)), 0);
            if (setting != NULL && setting->getExposureTime(settingExposure) == 0 && settingExposure != 0) {
                EXPECT_TRUE((abs((float)resultExposure / settingExposure - 1.0) < deltaExposure)) << "expected exposure " << settingExposure \
                                                           << ", actual " << resultExposure;
            }

            const float deltaGain = 0.02;  // %
            float settingGain = 0.0;
            float resultGain = 0.1;
            EXPECT_EQ((result.getSensitivityGain(resultGain)), 0);
            if (setting != NULL && setting->getSensitivityGain(settingGain) == 0 && abs(settingGain) > 0.00001) {
                EXPECT_TRUE((abs(resultGain / settingGain - 1.0) < deltaGain)) << "expected gain " << settingGain \
                                                           << ", actual " << resultGain;
            }

            return 0;
        }

        int getStreamConfiguration() {
            // TODO: find approprivate config from parameters
            if (prepareStreams(mCameraId, mStreams, mStreamNum) != 0) {
                camera_info_t info;
                int ret = get_camera_info(mCameraId, info);
                supported_stream_config_array_t configs;
                info.capability->getSupportedStreamConfig(configs);
                for (int i = 0; i < mStreamNum; i++) {
                    mStreams[i] = getStreamByConfig(configs[i]);
                    LOGD("@%s, preset stream not supported, use stream: format:%s (%dx%d) field=%d",
                            __func__,
                            CameraUtils::format2string(mStreams[i].format), mStreams[i].width,
                            mStreams[i].height, mStreams[i].field);
                }
            }
            return 0;
        }

        int allocateBuffers(int bufNum) {
            int ret = 0;
            const int page_size = getpagesize();
            mBufferCount = bufNum;
            for (int i = 0; i < mStreamNum; i++) {
                mBuffers[i] = new camera_buffer_t[mBufferCount];

                camera_buffer_t *buffer;
                int j = 0;
                for (j = 0, buffer = mBuffers[i]; j < mBufferCount; j++, buffer++) {
                    memset(buffer, 0, sizeof(camera_buffer_t));
                    stream_t stream = mStreams[i];
                    buffer->s = stream;
                    buffer->s.size = CameraUtils::getFrameSize(stream.format, stream.width, stream.height);
                    buffer->flags = 0;

                    ret = posix_memalign(&buffer->addr, page_size, buffer->s.size);
                    EXPECT_TRUE((buffer->addr != NULL) && (ret == 0));
                }
            }
            return ret;
        }

        void freeBuffers() {
            for (int i = 0; i < mStreamNum; i++) {
                camera_buffer_t *buffers = mBuffers[i];
                for (int j = 0; j < mBufferCount; j++) {
                    free(buffers[j].addr);
                    buffers[j].addr = NULL;
                }
                delete []mBuffers[i];
            }
            CLEAR(mBuffers);
        }

        int qBufToDevice(uint32_t frameNum, ParamList *params = NULL)
        {
            if (frameNum > mBufferCount)
                 return -1;

            int ret = 0;
            int frameIdx = 0;
            for (int i = 0; i < mStreamNum; i++) {
                for (frameIdx = 0; frameIdx < frameNum; frameIdx++) {
                    camera_buffer_t *buffer = &mBuffers[i][frameIdx];
                    if (params != NULL && params->find(frameIdx) != params->end()) {
                        Parameters p = params->at(frameIdx);
                        ret = camera_stream_qbuf(mCameraId, &buffer, 1, &p);
                    } else {
                        ret = camera_stream_qbuf(mCameraId, &buffer, 1);
                    }

                    if (ret != 0)
                        break;
                }
            }
            return ret;
        }

        int dqbufAndCheckParams(int framesNum, ParamList *params = NULL) {
            int ret = 0;

            camera_buffer_t *buffer = NULL;
            Parameters result[mStreamNum], *setting;
            for (int frameIdx = 0; frameIdx < framesNum; frameIdx++) {
                if (params != NULL && params->find(frameIdx) != params->end()) {
                    setting = &(params->at(frameIdx));
                } else {
                    setting = NULL;
                }
                for (int i = 0; i < mStreamNum; i++) {
                    ret = camera_stream_dqbuf(mCameraId, mStreams[i].id, &buffer, &result[i]);
                    EXPECT_EQ(ret, 0);

                    if (setting != NULL) {
                        ret = checkSensorParams(result[i], setting);
                        ret = checkIspParams(result[i], setting);
                    } else if (i == 0) {
                        ret = checkSensorParams(result[0]);
                        ret = checkIspParams(result[0]);
                    } else {
                        ret = checkSensorParams(result[i], &result[0]);
                        ret = checkIspParams(result[i], &result[0]);
                    }
                }
            }
            return ret;
        }

        int buildIspSettings(uint32_t paramsNum, IspParamType paramType = PARAM_AWB_GAIN, int frameInterval = 1) {
            if (paramsNum < 2) {
                LOGE("params num < 0!");
                return 0;
            }
            if (frameInterval < 1) {
                frameInterval = 1;
            }

            camera_awb_gains_t awbGains[3];
            // AwbGain range is 0-255, set 200 on 3 gains separately
            awbGains[0]= {200, 0, 0};
            awbGains[1]= {0, 200, 0};
            awbGains[2]= {0, 0, 200};
            camera_color_transform_t transforms[3];
            CLEAR(transforms);
            for (int i = 0; i < 3; i++) {
                // set color_transform as: {{x,0,0}, {0,x,0} {0,0,x}}, x=0.5/1/1.5 (range is -2 to 2)
                transforms[i].color_transform[0][0] = ((float)i + 1)/2;
                transforms[i].color_transform[1][1] = ((float)i + 1)/2;
                transforms[i].color_transform[2][2] = ((float)i + 1)/2;
            }

            Parameters settings;
            int frameId = 0;
            for (int i = 0; i < paramsNum; i ++) {
                frameId = i * frameInterval;
                if (frameId > MAX_FRAME_NUM) {
                    frameId = MAX_FRAME_NUM;
                    break;
                }

                if (mParams.find(frameId) != mParams.end()) {
                    settings = mParams[frameId];
                }
                if (paramType == PARAM_AWB_GAIN) {
                    settings.setAwbMode(AWB_MODE_MANUAL_GAIN);
                    settings.setAwbGains(awbGains[i % 3]);
                } else if (paramType == PARAM_AWB_TRANSFORM) {
                    settings.setAwbMode(AWB_MODE_MANUAL_COLOR_TRANSFORM);
                    settings.setColorTransform(transforms[i % 3]);
                }

                mParams[frameId] = settings;
                // Print for debug
                dumpIspSettings(frameId, settings);
            }

            return frameId;
        }

        void dumpIspSettings(int frameId, const Parameters &settings) {
            camera_awb_gains_t awbGain;
            settings.getAwbGains(awbGain);
            camera_color_transform_t transform;
            settings.getColorTransform(transform);
            LOGD("%s: frame %d, awbGains [%d, %d, %d], ccm [(%6.4f %6.4f %6.4f)  (%6.4f %6.4f %6.4f)  (%6.4f %6.4f %6.4f)]",
                    __func__, frameId, awbGain.r_gain, awbGain.g_gain, awbGain.b_gain,
                    transform.color_transform[0][0], transform.color_transform[0][1], transform.color_transform[0][2],
                    transform.color_transform[1][0], transform.color_transform[1][1], transform.color_transform[1][2],
                    transform.color_transform[2][0], transform.color_transform[2][1], transform.color_transform[2][2]);
        }

        int checkIspParams(const Parameters &result, const Parameters *setting = NULL) {
            camera_awb_gains_t resultGains;
            camera_awb_gains_t settingGains;
            EXPECT_EQ((result.getAwbGains(resultGains)), 0);
            if (setting != NULL && setting->getAwbGains(settingGains) == 0) {
                EXPECT_EQ(settingGains.r_gain, resultGains.r_gain) << "AwbGains r diff: expect: " \
                                                << settingGains.r_gain << "; actual: " << resultGains.r_gain;
                EXPECT_EQ(settingGains.g_gain, resultGains.g_gain) << "AwbGains g diff: expect: " \
                                                << settingGains.g_gain << "; actual: " << resultGains.g_gain;
                EXPECT_EQ(settingGains.b_gain, resultGains.b_gain) << "AwbGains b diff: expect: " \
                                                << settingGains.b_gain << "; actual: " << resultGains.b_gain;
            }

            camera_color_transform_t resultTransform;
            camera_color_transform_t settingTransform;
            EXPECT_EQ((result.getColorTransform(resultTransform)), 0);
            if (setting != NULL && setting->getColorTransform(settingTransform) == 0) {
                for (int i = 0; i < 9; i ++) {
                    float res = resultTransform.color_transform[i / 3][i % 3];
                    float set = settingTransform.color_transform[i / 3][i % 3];
                    EXPECT_TRUE(abs(res - set) < 0.001) << "ColorTransform [" << i/3 << "][" << i%3 << "]" \
                                                "diff: expect:" << set << "; actual:" << res ;
                }
            }

            return 0;
        }

        void test_per_frame_control_normal(int);

        int mCameraId;

        int mStreamNum = 1;
        stream_t mStreams[MAX_STREAM_NUM];

        int mBufferCount;
        camera_buffer_t *mBuffers[MAX_STREAM_NUM];

        ParamList mParams;

        camera_range_t mSensorExposures;
        camera_range_t mSensorGains;
};


TEST_F(CamPerFrameTest, camera_device_per_frame_result)
{
    if (!isFeatureSupported(PER_FRAME_CONTROL)) return;

    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_device_open(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = getStreamConfiguration();
    EXPECT_EQ(ret, 0);

    stream_config_t streamList;
    streamList.num_streams = mStreamNum;
    streamList.streams = mStreams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    ret = camera_device_config_streams(mCameraId, &streamList);
    EXPECT_EQ(ret, 0);

    ret = allocateBuffers(MAX_FRAME_NUM);
    EXPECT_EQ(ret, 0);

    // Loop 1
    ret = qBufToDevice(MAX_FRAME_NUM / 2);
    EXPECT_EQ(ret, 0);

    ret = camera_device_start(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(MAX_FRAME_NUM / 2);
    EXPECT_EQ(ret, 0);

    // Loop 2
    ret = qBufToDevice(MAX_FRAME_NUM / 2);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(MAX_FRAME_NUM / 2);
    EXPECT_EQ(ret, 0);

    ret = camera_device_stop(mCameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(mCameraId);

    freeBuffers();

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

void CamPerFrameTest::test_per_frame_control_normal(int frameNum)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_device_open(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = getStreamConfiguration();
    EXPECT_EQ(ret, 0);

    stream_config_t streamList;
    streamList.num_streams = mStreamNum;
    streamList.streams = mStreams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_NORMAL;
    ret = camera_device_config_streams(mCameraId, &streamList);
    EXPECT_EQ(ret, 0);

    ret = allocateBuffers(frameNum);
    EXPECT_EQ(ret, 0);

    // Loop 1: test all parameters
    ret = qBufToDevice(frameNum, &mParams);
    EXPECT_EQ(ret, 0);

    ret = camera_device_start(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(frameNum, &mParams);
    EXPECT_EQ(ret, 0);

    // Loop 2: test some parameters
    ret = qBufToDevice(frameNum/2, &mParams);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(frameNum/2, &mParams);
    EXPECT_EQ(ret, 0);

    ret = camera_device_stop(mCameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(mCameraId);

    freeBuffers();

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}

TEST_F(CamPerFrameTest, camera_device_per_frame_control_normal)
{
    if (!isFeatureSupported(PER_FRAME_CONTROL)) return;
    mStreamNum = 1;

    int frameNum = buildSensorSettings(MAX_FRAME_NUM);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }

    test_per_frame_control_normal(frameNum);
}


TEST_F(CamPerFrameTest, camera_device_per_frame_control_and_auto_combination)
{
    if (!isFeatureSupported(PER_FRAME_CONTROL)) return;

    int frameNum = buildSensorSettings(10);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }

    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    ret = camera_device_open(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = getStreamConfiguration();
    EXPECT_EQ(ret, 0);

    stream_config_t streamList;
    streamList.num_streams = mStreamNum;
    streamList.streams = mStreams;
    streamList.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
    ret = camera_device_config_streams(mCameraId, &streamList);
    EXPECT_EQ(ret, 0);

    ret = allocateBuffers(frameNum);
    EXPECT_EQ(ret, 0);

    // Loop 1: Auto control
    ret = qBufToDevice(frameNum, NULL);
    EXPECT_EQ(ret, 0);

    ret = camera_device_start(mCameraId);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(frameNum, NULL);
    EXPECT_EQ(ret, 0);

    // Loop 1: Per-frame control
    ret = qBufToDevice(frameNum/2, &mParams);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(frameNum/2, &mParams);
    EXPECT_EQ(ret, 0);

    // Loop 2: Auto control
    ret = qBufToDevice(frameNum/2, NULL);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(frameNum/2, NULL);
    EXPECT_EQ(ret, 0);

    // Loop 2: Per-frame control
    ret = qBufToDevice(frameNum/2, &mParams);
    EXPECT_EQ(ret, 0);

    ret = dqbufAndCheckParams(frameNum/2, &mParams);
    EXPECT_EQ(ret, 0);

    ret = camera_device_stop(mCameraId);
    EXPECT_EQ(ret, 0);

    camera_device_close(mCameraId);

    freeBuffers();

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}


TEST_F(CamPerFrameTest, camera_device_per_frame_control_normal_two_streams)
{
    if (!isFeatureSupported(PER_FRAME_CONTROL)) return;

    mStreamNum = 2;

    int frameNum = buildSensorSettings(MAX_FRAME_NUM);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }

    test_per_frame_control_normal(frameNum);
}

TEST_F(CamPerFrameTest, camera_device_per_frame_control_normal_IspParams)
{
    if (!isFeatureSupported(PER_FRAME_CONTROL)) return;

    int frameNum = buildIspSettings(MAX_FRAME_NUM, PARAM_AWB_GAIN);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }
    test_per_frame_control_normal(frameNum);

    mParams.clear();
    frameNum = buildIspSettings(MAX_FRAME_NUM, PARAM_AWB_TRANSFORM);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }
    test_per_frame_control_normal(frameNum);
}

TEST_F(CamPerFrameTest, camera_device_per_frame_control_normal_IspParams_two_streams)
{
    if (!isFeatureSupported(PER_FRAME_CONTROL)) return;

    mStreamNum = 2;

    int frameNum = buildIspSettings(MAX_FRAME_NUM, PARAM_AWB_GAIN);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }
    test_per_frame_control_normal(frameNum);

    mParams.clear();
    frameNum = buildIspSettings(MAX_FRAME_NUM, PARAM_AWB_TRANSFORM);
    if (frameNum == 0) {
        return;
    }
    if (frameNum > MAX_FRAME_NUM) {
        frameNum = MAX_FRAME_NUM;
    }
    test_per_frame_control_normal(frameNum);
}
