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

#define LOG_TAG "CASE_PARAMETER"

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>
#include <float.h>

#include "gtest/gtest.h"
#include "gtest/gtest-param-test.h"
#include "CameraMetadata.h"
#include "metadata/ParameterHelper.h"
#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "MockSysCall.h"
#include "Parameters.h"
#include "IspControl.h"
#include "ICamera.h"
#include "case_common.h"
#include "PlatformData.h"

using namespace icamera;

#define FINISH_USING_CAMERA_METADATA(m)                         \
    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL)); \
    free_icamera_metadata(m);
/**
 * To run all paramter related case, add --gtest_filter="*param_*"
 */

/**
 * Test if parameter can be created correctly
 */
TEST(camHalRawTest, param_basic_create) {
    Parameters param;
    camera_range_t fps_get;
    int ret = param.getFpsRange(fps_get);
    EXPECT_NE(OK, ret);

    camera_range_t fps_set = {10, 30};
    param.setFpsRange(fps_set);
    param.getFpsRange(fps_get);

    EXPECT_EQ(fps_get.min, 10);
    EXPECT_EQ(fps_get.max, 30);
}

/**
 * Test copy constuctor and assignment operator
 */
TEST(camHalRawTest, param_copy_and_assignment) {
    Parameters base;
    camera_range_t fps_set = {10, 30};
    base.setFpsRange(fps_set);

    Parameters copy_constructor(base);
    camera_range_t fps_get;
    copy_constructor.getFpsRange(fps_get);

    EXPECT_EQ(fps_get.min, 10);
    EXPECT_EQ(fps_get.max, 30);

    Parameters assignment;
    assignment = base;
    assignment.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 10);
    EXPECT_EQ(fps_get.max, 30);

    fps_set.min = 15;
    fps_set.max = 20;
    base.setFpsRange(fps_set);

    // copy_constructor should be still 10,30, not impacted by base
    copy_constructor.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 10);
    EXPECT_EQ(fps_get.max, 30);

    assignment.getFpsRange(fps_get);
    // assignment should be still 10,30, not impacted by base
    EXPECT_EQ(fps_get.min, 10);
    EXPECT_EQ(fps_get.max, 30);
}

/**
 * Test merge two instance of Parameters together
 */
TEST(camHalRawTest, param_merge_with_other) {
    Parameters param;
    camera_range_t fps_set = {15, 30};
    param.setFpsRange(fps_set);

    Parameters new_one;
    new_one.merge(param);

    camera_range_t fps_get;
    new_one.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 15);
    EXPECT_EQ(fps_get.max, 30);

    fps_set.min = 20;
    fps_set.max = 20;
    new_one.setFpsRange(fps_set);
    // fps should be updated to 15, 30 after merging
    new_one.merge(param);
    new_one.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 15);
    EXPECT_EQ(fps_get.max, 30);
}

/**
 * Test merge into parameter from an instance of CameraMetadata
 */
TEST(camHalRawTest, param_merge_from_metadata) {

    CameraMetadata metadata;
    float fps[2] = {15, 30};
    metadata.update(CAMERA_AE_TARGET_FPS_RANGE, fps, 2);

    Parameters new_one;
    ParameterHelper::merge(metadata, &new_one);
    camera_range_t fps_get;
    new_one.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 15);
    EXPECT_EQ(fps_get.max, 30);

    camera_range_t fps_set = {20, 20};
    new_one.setFpsRange(fps_set);
    // fps should be updated to 15, 30 after merging
    ParameterHelper::merge(metadata, &new_one);
    new_one.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 15);
    EXPECT_EQ(fps_get.max, 30);

    CameraMetadata copy_metadata;
    ParameterHelper::copyMetadata(new_one, &copy_metadata);
    EXPECT_EQ(metadata.entryCount(), metadata.entryCount());
}

TEST(camHalRawTest, param_operation_without_init) {
    Parameters param_set;
    camera_range_t fps_set = {10, 30};
    param_set.setFpsRange(fps_set);
    int ret = camera_set_parameters(0, param_set);
    EXPECT_NE(ret, OK);

    Parameters param_get;
    ret = camera_get_parameters(0, param_get);
    EXPECT_NE(ret, OK);
}

TEST(camHalRawTest, param_set_get_ae_mode) {
    Parameters param;
    camera_ae_mode_t aeModeIn = AE_MODE_MANUAL;
    camera_ae_mode_t aeModeOut;

    param.setAeMode(aeModeIn);
    param.getAeMode(aeModeOut);

    EXPECT_EQ(aeModeIn, aeModeOut);
}

TEST(camHalRawTest, param_set_get_ae_lock) {
    Parameters param;
    bool lockIn = true;
    bool lockOut = false;

    param.setAeLock(lockIn);
    param.getAeLock(lockOut);

    EXPECT_EQ(lockIn, lockOut);
}

TEST(camHalRawTest, param_set_get_exposure_time) {
    Parameters param;
    int64_t expTimeIn = 10 * 1000;
    int64_t expTimeOut = 0;

    param.setExposureTime(expTimeIn);
    param.getExposureTime(expTimeOut);
    EXPECT_EQ(expTimeIn, expTimeOut);
}

TEST(camHalRawTest, param_set_get_fisheye_dewarping_mode) {
    Parameters param;
    camera_fisheye_dewarping_mode_t modeIn = FISHEYE_DEWARPING_REARVIEW;
    camera_fisheye_dewarping_mode_t modeOut = FISHEYE_DEWARPING_OFF;

    param.setFisheyeDewarpingMode(modeIn);
    param.getFisheyeDewarpingMode(modeOut);
    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_video_stabilization_mode) {
    Parameters param;
    camera_video_stabilization_mode_t videoStabilizationModeIn = VIDEO_STABILIZATION_MODE_ON;
    camera_video_stabilization_mode_t videoStabilizationModeOut = VIDEO_STABILIZATION_MODE_OFF;

    param.setVideoStabilizationMode(videoStabilizationModeIn);
    param.getVideoStabilizationMode(videoStabilizationModeOut);

    EXPECT_EQ(videoStabilizationModeIn, videoStabilizationModeOut);
}

TEST(camHalRawTest, param_set_get_ldc_mode) {
    Parameters param;
    camera_ldc_mode_t ldcModeIn = LDC_MODE_ON;
    camera_ldc_mode_t ldcModeOut = LDC_MODE_OFF;

    param.setLdcMode(ldcModeIn);
    param.getLdcMode(ldcModeOut);

    EXPECT_EQ(ldcModeIn, ldcModeOut);
}

TEST(camHalRawTest, param_set_get_aperture) {
    Parameters param;
    float apertureIn  = 2.5;
    float apertureOut = 0.0;

    param.setAperture(apertureIn);
    param.getAperture(apertureOut);

    EXPECT_EQ(apertureIn, apertureOut);
}

TEST(camHalRawTest, param_set_get_rsc_mode) {
    Parameters param;
    camera_rsc_mode_t rscModeIn = RSC_MODE_ON;
    camera_rsc_mode_t rscModeOut = RSC_MODE_OFF;

    param.setRscMode(rscModeIn);
    param.getRscMode(rscModeOut);

    EXPECT_EQ(rscModeIn, rscModeOut);
}

TEST(camHalRawTest, param_set_get_digital_zoom_ratio) {
    Parameters param;
    float ratioIn = 2.5;
    float ratioOut = 1.0;

    param.setDigitalZoomRatio(ratioIn);
    param.getDigitalZoomRatio(ratioOut);

    EXPECT_EQ(ratioIn, ratioOut);
}

TEST(camHalRawTest, param_set_get_sensitivity_gain) {
    Parameters param;
    float gainIn = 10.5;
    float gainOut = 0.0;

    param.setSensitivityGain(gainIn);
    param.getSensitivityGain(gainOut);

    EXPECT_EQ(gainIn, gainOut);
}

TEST(camHalRawTest, param_set_get_ae_compensation) {
    Parameters param;
    int evIn = -2;
    int evOut = 0;

    param.setAeCompensation(evIn);
    param.getAeCompensation(evOut);

    EXPECT_EQ(evIn, evOut);
}

TEST(camHalRawTest, param_set_get_frame_rate) {
    Parameters param;
    float fpsIn = 30;
    float fpsOut = 0;

    param.setFrameRate(fpsIn);
    param.getFrameRate(fpsOut);

    EXPECT_EQ(fpsIn, fpsOut);
}

TEST(camHalRawTest, param_set_get_anti_banding_mode) {
    Parameters param;
    camera_antibanding_mode_t modeIn = ANTIBANDING_MODE_60HZ;
    camera_antibanding_mode_t modeOut = ANTIBANDING_MODE_OFF;

    param.setAntiBandingMode(modeIn);
    param.getAntiBandingMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_ae_state) {
    Parameters param;
    camera_ae_state_t stateIn = AE_STATE_CONVERGED;
    camera_ae_state_t stateOut = AE_STATE_NOT_CONVERGED;

    param.setAeState(stateIn);
    param.getAeState(stateOut);

    EXPECT_EQ(stateIn, stateOut);
}

TEST(camHalRawTest, param_set_get_awb_state) {
    Parameters param;
    camera_awb_state_t stateIn = AWB_STATE_CONVERGED;
    camera_awb_state_t stateOut = AWB_STATE_NOT_CONVERGED;

    param.setAwbState(stateIn);
    param.getAwbState(stateOut);

    EXPECT_EQ(stateIn, stateOut);
}

TEST(camHalRawTest, param_set_get_af_state) {
    Parameters param;
    camera_af_state_t stateIn = AF_STATE_SUCCESS;
    camera_af_state_t stateOut = AF_STATE_LOCAL_SEARCH;

    param.setAfState(stateIn);
    param.getAfState(stateOut);

    EXPECT_EQ(stateIn, stateOut);
}

TEST(camHalRawTest, param_set_get_lens_state) {
    Parameters param;
    bool stateIn = true;
    bool stateOut = false;

    param.setLensState(stateIn);
    param.getLensState(stateOut);

    EXPECT_EQ(stateIn, stateOut);
}

void checkAeAwbRegions(const camera_window_list_t &regionsIn, const camera_window_list_t &regionsOut)
{
    for (size_t i = 0; i < regionsIn.size(); i++) {
        EXPECT_EQ(regionsIn[i].left, regionsOut[i].left);
        EXPECT_EQ(regionsIn[i].top, regionsOut[i].top);
        EXPECT_EQ(regionsIn[i].right, regionsOut[i].right);
        EXPECT_EQ(regionsIn[i].bottom, regionsOut[i].bottom);
        EXPECT_EQ(regionsIn[i].weight, regionsOut[i].weight);
    }
}

TEST(camHalRawTest, param_set_get_ae_regions) {
    Parameters param;
    camera_window_list_t regionsIn {{100, 100, 150, 150, 1},
                                    {200, 200, 300, 300, 2}};
    camera_window_list_t regionsOut;

    param.setAeRegions(regionsIn);
    param.getAeRegions(regionsOut);

    checkAeAwbRegions(regionsIn, regionsOut);
}

TEST(camHalRawTest, param_set_get_awb_mode) {
    Parameters param;
    camera_awb_mode_t modeIn = AWB_MODE_FLUORESCENT;
    camera_awb_mode_t modeOut = AWB_MODE_AUTO;

    param.setAwbMode(modeIn);
    param.getAwbMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_awb_lock) {
    Parameters param;
    bool lockIn = true;
    bool lockOut = false;

    param.setAwbLock(lockIn);
    param.getAwbLock(lockOut);

    EXPECT_EQ(lockIn, lockOut);
}

TEST(camHalRawTest, param_set_get_cct_range) {
    Parameters param;
    camera_range_t rangeIn = {5000, 6000};
    camera_range_t rangeOut;

    param.setAwbCctRange(rangeIn);
    param.getAwbCctRange(rangeOut);

    EXPECT_EQ(rangeIn.min, rangeOut.min);
    EXPECT_EQ(rangeIn.max, rangeOut.max);
}

TEST(camHalRawTest, param_set_get_awb_gains) {
    Parameters param;
    camera_awb_gains_t gainsIn = {10, 20, 30};
    camera_awb_gains_t gainsOut;

    param.setAwbGains(gainsIn);
    param.getAwbGains(gainsOut);

    EXPECT_EQ(gainsIn.r_gain, gainsOut.r_gain);
    EXPECT_EQ(gainsIn.g_gain, gainsOut.g_gain);
    EXPECT_EQ(gainsIn.b_gain, gainsOut.b_gain);
}

TEST(camHalRawTest, param_set_get_awb_gain_shift) {
    Parameters param;
    camera_awb_gains_t gainsIn = {10, 20, 30};
    camera_awb_gains_t gainsOut;

    param.setAwbGainShift(gainsIn);
    param.getAwbGainShift(gainsOut);

    EXPECT_EQ(gainsIn.r_gain, gainsOut.r_gain);
    EXPECT_EQ(gainsIn.g_gain, gainsOut.g_gain);
    EXPECT_EQ(gainsIn.b_gain, gainsOut.b_gain);
}

TEST(camHalRawTest, param_set_get_awb_result) {
    Parameters param;
    camera_awb_result_t resultIn = {0.5, 0.4};
    camera_awb_result_t resultOut;
    CLEAR(resultOut);

    param.setAwbResult(&resultIn);
    param.getAwbResult(&resultOut);

    EXPECT_EQ(resultIn.r_per_g, resultOut.r_per_g);
    EXPECT_EQ(resultIn.b_per_g, resultOut.b_per_g);
}

TEST(camHalRawTest, param_set_get_awb_white_point) {
    Parameters param;
    camera_coordinate_t pointIn = {100, 200};
    camera_coordinate_t pointOut;

    param.setAwbWhitePoint(pointIn);
    param.getAwbWhitePoint(pointOut);

    EXPECT_EQ(pointIn.x, pointOut.x);
    EXPECT_EQ(pointIn.y, pointOut.y);
}

TEST(camHalRawTest, param_set_get_color_transform) {
    Parameters param;
    camera_color_transform_t transformIn;
    camera_color_transform_t transformOut;

    for (int i = 0; i < 9; i++) {
        transformIn.color_transform[i / 3][i % 3] = (i + 1) * 1.1;
    }
    param.setColorTransform(transformIn);
    param.getColorTransform(transformOut);

    for (int i = 0; i < 9; i++) {
        EXPECT_EQ(transformIn.color_transform[i / 3][i % 3], transformOut.color_transform[i / 3][i % 3]);
    }
}

TEST(camHalRawTest, param_set_get_awb_regions) {
    Parameters param;
    camera_window_list_t regionsIn {{100, 100, 150, 150, 1},
                                    {200, 200, 300, 300, 2}};
    camera_window_list_t regionsOut;

    param.setAwbRegions(regionsIn);
    param.getAwbRegions(regionsOut);

    checkAeAwbRegions(regionsIn, regionsOut);
}

TEST(camHalRawTest, param_set_get_af_trigger) {
    Parameters param;
    camera_af_trigger_t triggerIn = AF_TRIGGER_START;
    camera_af_trigger_t triggerOut = AF_TRIGGER_IDLE;

    param.setAfTrigger(triggerIn);
    param.getAfTrigger(triggerOut);

    EXPECT_EQ(triggerIn, triggerOut);
}

TEST(camHalRawTest, param_set_get_nr_mode) {
    Parameters param;
    camera_nr_mode_t modeIn = NR_MODE_MANUAL_NORMAL;
    camera_nr_mode_t modeOut = NR_MODE_AUTO;

    param.setNrMode(modeIn);
    param.getNrMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_nr_level) {
    Parameters param;
    camera_nr_level_t levelIn = {100, 200, 300};
    camera_nr_level_t levelOut;

    param.setNrLevel(levelIn);
    param.getNrLevel(levelOut);

    EXPECT_EQ(levelIn.overall, levelOut.overall);
    EXPECT_EQ(levelIn.spatial, levelOut.spatial);
    EXPECT_EQ(levelIn.temporal, levelOut.temporal);
}

TEST(camHalRawTest, param_set_get_iris_mode) {
    Parameters param;
    camera_iris_mode_t modeIn = IRIS_MODE_CUSTOMIZED;
    camera_iris_mode_t modeOut = IRIS_MODE_AUTO;

    param.setIrisMode(modeIn);
    param.getIrisMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_iris_level) {
    Parameters param;
    int levelIn = 100;
    int levelOut = 0;

    param.setIrisLevel(levelIn);
    param.getIrisLevel(levelOut);

    EXPECT_EQ(levelIn, levelOut);
}

TEST(camHalRawTest, param_set_get_wdr_mode) {
    Parameters param;
    camera_wdr_mode_t modeIn = WDR_MODE_AUTO;
    camera_wdr_mode_t modeOut = WDR_MODE_OFF;

    param.setWdrMode(modeIn);
    param.getWdrMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}


TEST(camHalRawTest, param_set_get_makernote_mode) {
    Parameters param;
    camera_makernote_mode_t modeIn = MAKERNOTE_MODE_JPEG;
    camera_makernote_mode_t modeOut = MAKERNOTE_MODE_OFF;

    param.setMakernoteMode(modeIn);
    param.getMakernoteMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_wdr_level) {
    Parameters param;
    uint8_t levelIn = 100;
    uint8_t levelOut = 0;

    param.setWdrLevel(levelIn);
    param.getWdrLevel(levelOut);

    EXPECT_EQ(levelIn, levelOut);
}

TEST(camHalRawTest, param_set_get_scene_mode) {
    Parameters param;
    camera_scene_mode_t modeIn = SCENE_MODE_HDR;
    camera_scene_mode_t modeOut = SCENE_MODE_MAX;

    param.setSceneMode(modeIn);
    param.getSceneMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_blc_area_mode) {
    Parameters param;
    camera_blc_area_mode_t modeIn = BLC_AREA_MODE_ON;
    camera_blc_area_mode_t modeOut = BLC_AREA_MODE_OFF;

    param.setBlcAreaMode(modeIn);
    param.getBlcAreaMode(modeOut);

    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_image_enhancement) {
    Parameters param;
    camera_image_enhancement_t enhancementIn = {10, 20, 30, 40, 50}; // Just fill some random values
    param.setImageEnhancement(enhancementIn);

    camera_image_enhancement_t enhancementOut;
    param.getImageEnhancement(enhancementOut);

    // Verify if each field matches.
    EXPECT_EQ(enhancementIn.sharpness, enhancementOut.sharpness);
    EXPECT_EQ(enhancementIn.brightness, enhancementOut.brightness);
    EXPECT_EQ(enhancementIn.contrast, enhancementOut.contrast);
    EXPECT_EQ(enhancementIn.hue, enhancementOut.hue);
    EXPECT_EQ(enhancementIn.saturation, enhancementOut.saturation);
}

TEST(camHalRawTest, param_set_get_ae_compensation_range) {
    CameraMetadata metadata;
    int evRangeSet[] = {-3, 3};
    metadata.update(CAMERA_AE_COMPENSATION_RANGE, evRangeSet, ARRAY_SIZE(evRangeSet));

    Parameters param;
    ParameterHelper::merge(metadata, &param);

    camera_range_t evRangeGet;
    param.getAeCompensationRange(evRangeGet);

    EXPECT_EQ(evRangeGet.min, evRangeSet[0]);
    EXPECT_EQ(evRangeGet.max, evRangeSet[1]);
}

TEST(camHalRawTest, param_set_get_ae_compensation_step) {
    CameraMetadata metadata;
    icamera_metadata_rational_t evStepSet = {1, 3};
    metadata.update(CAMERA_AE_COMPENSATION_STEP, &evStepSet, 1);

    Parameters param;
    ParameterHelper::merge(metadata, &param);
    camera_rational_t evStepGet;
    param.getAeCompensationStep(evStepGet);

    EXPECT_EQ(evStepGet.numerator, evStepSet.numerator);
    EXPECT_EQ(evStepGet.denominator, evStepSet.denominator);
}

TEST(camHalRawTest, param_set_get_ae_converge_spped) {
    Parameters param;
    camera_converge_speed_t speedIn = CONVERGE_NORMAL;
    camera_converge_speed_t speedOut = CONVERGE_LOW;

    param.setAeConvergeSpeed(speedIn);
    param.getAeConvergeSpeed(speedOut);
    EXPECT_EQ(speedIn, speedOut);

    speedIn = CONVERGE_MID;
    param.setAeConvergeSpeed(speedIn);
    param.getAeConvergeSpeed(speedOut);
    EXPECT_EQ(speedIn, speedOut);

    speedIn = CONVERGE_LOW;
    param.setAeConvergeSpeed(speedIn);
    param.getAeConvergeSpeed(speedOut);
    EXPECT_EQ(speedIn, speedOut);
}

TEST(camHalRawTest, param_set_get_awb_converge_spped) {
    Parameters param;
    camera_converge_speed_t speedIn = CONVERGE_NORMAL;
    camera_converge_speed_t speedOut = CONVERGE_LOW;

    param.setAwbConvergeSpeed(speedIn);
    param.getAwbConvergeSpeed(speedOut);
    EXPECT_EQ(speedIn, speedOut);

    speedIn = CONVERGE_MID;
    param.setAwbConvergeSpeed(speedIn);
    param.getAwbConvergeSpeed(speedOut);
    EXPECT_EQ(speedIn, speedOut);

    speedIn = CONVERGE_LOW;
    param.setAwbConvergeSpeed(speedIn);
    param.getAwbConvergeSpeed(speedOut);
    EXPECT_EQ(speedIn, speedOut);
}

TEST(camHalRawTest, param_set_get_ae_converge_spped_mode) {
    Parameters param;
    camera_converge_speed_mode_t modeIn = CONVERGE_SPEED_MODE_HAL;
    camera_converge_speed_mode_t modeOut = CONVERGE_SPEED_MODE_AIQ;

    param.setAeConvergeSpeedMode(modeIn);
    param.getAeConvergeSpeedMode(modeOut);
    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_awb_converge_spped_mode) {
    Parameters param;
    camera_converge_speed_mode_t modeIn = CONVERGE_SPEED_MODE_HAL;
    camera_converge_speed_mode_t modeOut = CONVERGE_SPEED_MODE_AIQ;

    param.setAwbConvergeSpeedMode(modeIn);
    param.getAwbConvergeSpeedMode(modeOut);
    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_deinterlace_mode) {
    Parameters param;
    camera_deinterlace_mode_t modeIn = DEINTERLACE_WEAVING;
    camera_deinterlace_mode_t modeOut = DEINTERLACE_OFF;

    param.setDeinterlaceMode(modeIn);
    param.getDeinterlaceMode(modeOut);
    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_makernote_data) {
    Parameters param;
    const unsigned int size = MAKERNOTE_SECTION1_SIZE + MAKERNOTE_SECTION2_SIZE;
    char* src = new char[size];
    const char* str = "Intel Makernote Data are good!";
    MEMCPY_S(src, size, str, strlen(str));

    char* dst = new char[size];
    unsigned int dstSize = size;

    param.setMakernoteData(src, size);
    param.getMakernoteData(dst, &dstSize);

    EXPECT_EQ(size, dstSize);
    for(int i = 0; i < strlen(str); i++) {
        EXPECT_EQ(src[i], dst[i]);
    }
}

TEST(camHalRawTest, param_set_get_custom_aic_control) {
    Parameters param;
    const char* src = "1,20";
    char dst[10];
    unsigned int length = sizeof(dst);
    CLEAR(dst);

    param.setCustomAicParam(src, strlen(src));
    param.getCustomAicParam(dst, &length);

    EXPECT_EQ(strlen(src), length);
    for (int i = 0; i < length; i++) {
        EXPECT_EQ(src[i], dst[i]);
    }
}

TEST(camHalRawTest, param_set_get_yuv_color_range_mode) {
    Parameters param;
    camera_yuv_color_range_mode_t modeIn = CAMERA_FULL_MODE_YUV_COLOR_RANGE;
    camera_yuv_color_range_mode_t modeOut = CAMERA_REDUCED_MODE_YUV_COLOR_RANGE;

    param.setYuvColorRangeMode(modeIn);
    param.getYuvColorRangeMode(modeOut);
    EXPECT_EQ(modeIn, modeOut);
}

TEST(camHalRawTest, param_set_get_exposure_time_range) {
    Parameters param;
    camera_range_t rangeIn = {100, 33333};
    camera_range_t rangeOut = {100, 33333};

    int ret = param.setExposureTimeRange(rangeIn);
    EXPECT_EQ(OK, ret);

    ret = param.getExposureTimeRange(rangeOut);
    EXPECT_EQ(OK, ret);

    EXPECT_EQ(rangeOut.min, rangeIn.min);
    EXPECT_EQ(rangeOut.max, rangeIn.max);
}

TEST(camHalRawTest, param_set_get_sensitivity_gain_range) {
    Parameters param;
    camera_range_t rangeIn = {5.5, 60.8};
    camera_range_t rangeOut = {5.5, 60.8};

    int ret = param.setSensitivityGainRange(rangeIn);
    EXPECT_EQ(OK, ret);

    ret = param.getSensitivityGainRange(rangeOut);
    EXPECT_EQ(OK, ret);

    EXPECT_EQ(rangeOut.min, rangeIn.min);
    EXPECT_EQ(rangeOut.max, rangeIn.max);
}

TEST(camHalRawTest, param_set_get_isp_control) {
    Parameters param;
    camera_control_isp_wb_gains_t wbGainSet = {1.0, 2.0, 3.0, 4.0};
    int ret = param.setIspControl(camera_control_isp_ctrl_id_wb_gains, &wbGainSet);
    EXPECT_EQ(OK, ret);

    camera_control_isp_wb_gains_t wbGainGet;
    ret = param.getIspControl(camera_control_isp_ctrl_id_wb_gains, &wbGainGet);
    EXPECT_EQ(OK, ret);

    EXPECT_EQ(wbGainSet.gr, wbGainGet.gr);
    EXPECT_EQ(wbGainSet.r, wbGainGet.r);
    EXPECT_EQ(wbGainSet.b, wbGainGet.b);
    EXPECT_EQ(wbGainSet.gb, wbGainGet.gb);
}

TEST(camHalRawTest, param_set_get_enabled_isp_controls) {
    Parameters param;
    set<uint32_t> ctrlIdsSet = {camera_control_isp_ctrl_id_wb_gains,
                                camera_control_isp_ctrl_id_gamma_tone_map};
    int ret = param.setEnabledIspControls(ctrlIdsSet);
    EXPECT_EQ(OK, ret);

    set<uint32_t> ctrlIdsGet;
    ret = param.getEnabledIspControls(ctrlIdsGet);
    EXPECT_EQ(OK, ret);

    EXPECT_TRUE(ctrlIdsSet == ctrlIdsGet);
}

TEST(camHalRawTest, param_set_get_ltm_tuning_data) {
    Parameters param;

    const int dataSize = 264; // FIXME: Will be fixed after turning data header file released.
    char dataSet[dataSize];
    for (int i = 0; i < dataSize; i++) {
        dataSet[i] = i % 100;
    }
    int ret = param.setLtmTuningData(dataSet);
    EXPECT_EQ(OK, ret);

    char dataGet[dataSize] = { 0 };
    ret = param.getLtmTuningData(dataGet);
    EXPECT_EQ(OK, ret);

    for (int i = 0; i < dataSize; i++) {
        EXPECT_EQ(dataSet[i], dataGet[i]);
    }

    ret = param.getLtmTuningData(nullptr);
    EXPECT_EQ(OK, ret);

    ret = param.setLtmTuningData(nullptr);
    EXPECT_EQ(OK, ret);

    ret = param.getLtmTuningData(dataGet);
    EXPECT_NE(OK, ret);
}

#if defined(HAVE_PTHREADS)
#include <pthread.h>

static void* manipulate_parameter(void* parameter)
{
    Parameters* p = (Parameters*) parameter;
    EXPECT_NOT_NULL(p);
    for (int i = 0; i < 100000; i++) {
        // Just add some random operations here.
        camera_range_t fps_set = {10, 30};
        camera_range_t fps_get;
        p->setFpsRange(fps_set);
        p->getFpsRange(fps_get);
        Parameters tmp;
        camera_range_t fps_set_1 = {15, 30};
        tmp.setFpsRange(fps_set_1);
        p->merge(tmp);
        CameraMetadata metadata;
        supported_stream_config_t cfg;
        CLEAR(cfg);
        cfg.format = V4L2_PIX_FMT_NV12;
        cfg.width = 1920;
        cfg.height = 1080;
        cfg.maxVideoFps = 30;
        cfg.maxCaptureFps = 30;
        int config[sizeof(supported_stream_config_t) / sizeof(int)];
        MEMCPY_S(config, sizeof(supported_stream_config_t), &cfg, sizeof(supported_stream_config_t));
        metadata.update(CAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS, config, ARRAY_SIZE(config));
        ParameterHelper::merge(metadata, p);
        supported_stream_config_array_t configs;
        p->getSupportedStreamConfig(configs);
        EXPECT_NE(configs.size(), 0);
    }
    return NULL;
}

/**
 * Test stability in multiple threads case, only available when pthread is available
 */
TEST(camHalRawTest, param_multi_thread_operation) {
    const char* DEBUG_KEY = "cameraDebug";
    const char* debug_value = getenv(DEBUG_KEY);
    if (debug_value) {
        setenv(DEBUG_KEY, "0", 1);
        Log::setDebugLevel();
    }
    Parameters param;
    pthread_t thread1, thread2, thread3;
    pthread_create(&thread1, NULL, manipulate_parameter, (void*)&param);
    pthread_create(&thread2, NULL, manipulate_parameter, (void*)&param);
    pthread_create(&thread3, NULL, manipulate_parameter, (void*)&param);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);
    pthread_join(thread3, NULL);
    camera_range_t fps_get;
    param.getFpsRange(fps_get);
    EXPECT_EQ(fps_get.min, 15);
    EXPECT_EQ(fps_get.max, 30);
    if (debug_value) {
        setenv(DEBUG_KEY, debug_value, 1);
        Log::setDebugLevel();
    }
}

#endif // HAVE_PTHREADS

class camStaticParam: public testing::Test {
protected:
    virtual void SetUp() {
        camera_info_t info;
        cameraId = getCurrentCameraId();
        int ret = PlatformData::getCameraInfo(cameraId, info);
        EXPECT_EQ(OK, ret);
        EXPECT_NOT_NULL(info.capability);
        param = info.capability;
    }

    virtual void TearDown() {

    }

    const Parameters *param;
    int cameraId;
    int ret;
};

TEST_F(camStaticParam, param_get_supported_video_stabilization_mode) {
    camera_video_stabilization_list_t support_mode;
    camera_video_stabilization_mode_t mode;

    ret = param->getSupportedVideoStabilizationMode(support_mode);
    // Skip the test if video stabilization mode is not supported.
    if (OK != ret || 0 == support_mode.size()) return;

    LOGD("Camera id:%d. Support video stabilization mode list(%zu): ",
            cameraId, support_mode.size());
    for (auto mode : support_mode) {
        LOGD("%d, ", mode);
        EXPECT_TRUE(mode <= VIDEO_STABILIZATION_MODE_ON);
    }
    LOGD("\n");
}

TEST_F(camStaticParam, param_get_supported_ae_mode) {
    vector <camera_ae_mode_t> support_mode;
    camera_ae_mode_t mode;

    ret = param->getSupportedAeMode(support_mode);
    // Skip the test if ae mode is not supported.
    if (OK != ret || 0 == support_mode.size()) return;

    LOGD("Camera id:%d. Support ae mode list(%zu): ", cameraId, support_mode.size());
    for (auto mode : support_mode) {
        LOGD("%d, ", mode);
        EXPECT_TRUE(mode < AE_MODE_MAX);
    }
    LOGD("\n");
}

TEST_F(camStaticParam, param_get_supported_awb_mode) {
    vector <camera_awb_mode_t> support_mode;
    camera_awb_mode_t mode;

    ret = param->getSupportedAwbMode(support_mode);
    // Skip the test if awb mode is not supported.
    if (OK != ret || 0 == support_mode.size()) return;

    LOGD("Camera id:%d. Support awb mode list(%zu): ", cameraId, support_mode.size());
    for (auto mode : support_mode) {
        LOGD("%d, ", mode);
        EXPECT_TRUE(mode < AWB_MODE_MAX);
    }
    LOGD("\n");
}

TEST_F(camStaticParam, param_get_supported_af_mode) {
    vector <camera_af_mode_t> support_mode;
    camera_af_mode_t set_mode;
    camera_af_mode_t get_mode;
    Parameters param_dynamic;

    ret = param->getSupportedAfMode(support_mode);
    // Skip the test if af mode is not supported.
    if (OK != ret || 0 == support_mode.size()) return;

    LOGD("Camera id:%d. Support af mode list(%zu):\n", cameraId, support_mode.size());
    for (auto set_mode : support_mode) {
        LOGD("%d, ", set_mode);
        EXPECT_TRUE(set_mode < AF_MODE_MAX);

        EXPECT_EQ(OK, param_dynamic.setAfMode(set_mode));
        EXPECT_EQ(OK, param_dynamic.getAfMode(get_mode));
        EXPECT_EQ(set_mode, get_mode);
    }

    EXPECT_EQ(BAD_VALUE,param_dynamic.setAfMode((camera_af_mode_t)99));
    LOGD("\n");
}

TEST_F(camStaticParam, param_get_supported_scene_mode) {
    vector <camera_scene_mode_t> support_mode;
    camera_scene_mode_t mode;

    ret = param->getSupportedSceneMode(support_mode);
    // Skip the test if scene mode is not supported.
    if (OK != ret || 0 == support_mode.size()) return;

    LOGD("Camera id:%d. Support Scene mode list(%zu):\n", cameraId, support_mode.size());
    for (auto mode : support_mode) {
        LOGD("%d, ", mode);
        EXPECT_TRUE(mode < SCENE_MODE_MAX);
    }
    LOGD("\n");
}

TEST_F(camStaticParam, param_get_supported_antibanding_mode) {
    vector <camera_antibanding_mode_t> support_mode;
    camera_antibanding_mode_t mode;

    ret = param->getSupportedAntibandingMode(support_mode);
    // Skip the test if antibanding mode is not supported.
    if (OK != ret || 0 == support_mode.size()) return;

    LOGD("Camera id:%d. Support Antibanding mode list(%zu):\n", cameraId, support_mode.size());
    for (auto mode : support_mode) {
        LOGD("%d, ", mode);
        EXPECT_TRUE(mode <= ANTIBANDING_MODE_OFF);
    }
    LOGD("\n");
}

TEST_F(camStaticParam, param_get_supported_sensor_exposure_time_range) {
    camera_range_t support_range;

    ret = param->getSupportedSensorExposureTimeRange(support_range);
    // Skip the test if sensor exposure time range is not supported.
    if (OK != ret || (0 == support_range.min && 0 == support_range.max)) return;

    EXPECT_LE(support_range.min, support_range.max);

    LOGD("Camera id:%d. Support sensor exposure time range: min(%f), max(%f)\n",
            cameraId, support_range.min, support_range.max);
}

TEST_F(camStaticParam, param_get_supported_sensor_sensitivity_range) {
    camera_range_t support_range;

    ret = param->getSupportedSensorSensitivityRange(support_range);
    // Skip the test if sensor sensitivity range is not supported.
    if (OK != ret || (0 == support_range.min && 0 == support_range.max)) return;

    ASSERT_LE(support_range.min, 100);
    ASSERT_GE(support_range.max, 800);

    LOGD("Camera id:%d. Support sensor sensitivity range: min(%f), max(%f)",
            cameraId, support_range.min, support_range.max);
}

#if GTEST_HAS_PARAM_TEST

using ::testing::TestWithParam;


class camDynamicParamInt : public TestWithParam<int> {
public:
    virtual void SetUp() {
        set_param = GetParam();
        get_param = 0;
    }
    virtual void TearDown() {
        EXPECT_EQ(set_param, get_param);
    }
protected:
    Parameters param;
    int set_param;
    int get_param;
};

TEST_P(camDynamicParamInt, jpeg_rotation) {
    EXPECT_EQ(NAME_NOT_FOUND, param.getJpegRotation(get_param));
    EXPECT_EQ(OK, param.setJpegRotation(set_param));
    EXPECT_EQ(OK, param.getJpegRotation(get_param));
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamInt, testing::Values(70));

class camDynamicParamUint8 : public TestWithParam<uint8_t> {
public:
    virtual void SetUp() {
        set_param = GetParam();
        get_param = 0;
    }
    virtual void TearDown() {
        EXPECT_EQ(set_param, get_param);
    }
protected:
    Parameters param;
    uint8_t set_param;
    uint8_t get_param;
};

TEST_P(camDynamicParamUint8, jpeg_quality) {
    EXPECT_EQ(NAME_NOT_FOUND, param.getJpegQuality(&get_param));
    EXPECT_EQ(OK, param.setJpegQuality(set_param));
    EXPECT_EQ(OK, param.getJpegQuality(&get_param));
}

TEST_P(camDynamicParamUint8, jpeg_thumbnail_qualit) {
    EXPECT_EQ(NAME_NOT_FOUND, param.getJpegThumbnailQuality(&get_param));
    EXPECT_EQ(OK, param.setJpegThumbnailQuality(set_param));
    EXPECT_EQ(OK, param.getJpegThumbnailQuality(&get_param));
}

TEST_P(camDynamicParamUint8, af_mode) {
    camera_af_mode_t set_parameter = (camera_af_mode_t)set_param;
    camera_af_mode_t get_parameter;
    EXPECT_EQ(NAME_NOT_FOUND, param.getAfMode(get_parameter));
    EXPECT_EQ(OK, param.setAfMode(set_parameter));
    EXPECT_EQ(OK, param.getAfMode(get_parameter));
    get_param = (uint8_t)get_parameter;
}

TEST_P(camDynamicParamUint8, image_effect) {
    camera_effect_mode_t set_parameter = (camera_effect_mode_t)set_param;
    camera_effect_mode_t get_parameter;
    EXPECT_EQ(NAME_NOT_FOUND, param.getImageEffect(get_parameter));
    EXPECT_EQ(OK, param.setImageEffect(set_parameter));
    EXPECT_EQ(OK, param.getImageEffect(get_parameter));
    get_param = (uint8_t)get_parameter;
}

TEST_P(camDynamicParamUint8, ae_distribution_priority) {
    camera_ae_distribution_priority_t set_parameter = (camera_ae_distribution_priority_t)set_param;
    camera_ae_distribution_priority_t get_parameter;
    EXPECT_EQ(NAME_NOT_FOUND, param.getAeDistributionPriority(get_parameter));
    EXPECT_EQ(OK, param.setAeDistributionPriority(set_parameter));
    EXPECT_EQ(OK, param.getAeDistributionPriority(get_parameter));
    get_param = (uint8_t)get_parameter;
}

TEST_P(camDynamicParamUint8, effect_scene_mode) {
    camera_scene_mode_t set_parameter = (camera_scene_mode_t)set_param;
    camera_scene_mode_t get_parameter;
    EXPECT_EQ(NAME_NOT_FOUND, param.getEffectSceneMode(get_parameter));
    EXPECT_EQ(OK, param.setEffectSceneMode(set_parameter));
    EXPECT_EQ(OK, param.getEffectSceneMode(get_parameter));
    get_param = (uint8_t)get_parameter;
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamUint8, testing::Values(5));

class camDynamicParamInt64 : public TestWithParam<int64_t> {
public:
    virtual void SetUp() {
        set_param = GetParam();
        get_param = 0;
    }
    virtual void TearDown() {
        EXPECT_DOUBLE_EQ(set_param, get_param);
    }
protected:
    Parameters param;
    int64_t set_param;
    int64_t get_param;
};

TEST_P(camDynamicParamInt64, jpeg_time_stamp) {
    EXPECT_EQ(NAME_NOT_FOUND, param.getJpegGpsTimeStamp(get_param));
    EXPECT_EQ(OK, param.setJpegGpsTimeStamp(set_param));
    EXPECT_EQ(OK, param.getJpegGpsTimeStamp(get_param));
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamInt64, testing::Values(1522202859));

class camDynamicParamFloat : public TestWithParam<float> {
public:
    virtual void SetUp() {
        set_param = GetParam();
        get_param = 0;
    }
    virtual void TearDown() {
        EXPECT_FLOAT_EQ(set_param, get_param);
    }
protected:
    Parameters param;
    float set_param;
    float get_param;
};

TEST_P(camDynamicParamFloat, focall_length) {
    EXPECT_EQ(NAME_NOT_FOUND, param.getFocalLength(get_param));
    EXPECT_EQ(OK, param.setFocalLength(set_param));
    EXPECT_EQ(OK, param.getFocalLength(get_param));
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamFloat, testing::Values(35.5));

class camDynamicParamResolution : public TestWithParam< ::std::tr1::tuple<int, int> > {
public:
    virtual void SetUp() {
        set_param.width = ::std::tr1::get<0>(GetParam());
        set_param.height = ::std::tr1::get<1>(GetParam());
        get_param.width = 0;
        get_param.height = 0;
    }
    virtual void TearDown() {
        EXPECT_EQ(set_param.width, get_param.width);
        EXPECT_EQ(set_param.height, get_param.height);
    }
protected:
    Parameters param;
    camera_resolution_t set_param;
    camera_resolution_t get_param;
};


TEST_P(camDynamicParamResolution, jpeg_thumbnail_size) {
    EXPECT_EQ(OK, param.setJpegThumbnailSize(set_param));
    EXPECT_EQ(OK, param.getJpegThumbnailSize(get_param));
    EXPECT_EQ(set_param.height, get_param.height);
    EXPECT_EQ(set_param.width, get_param.width);
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamResolution, testing::Combine(
        testing::Values(2), testing::Values(1)));

class camDynamicParamCoordinates : public TestWithParam< ::std::tr1::tuple<double, double, double> > {
public:
    virtual void SetUp() {
        set_param[0] = ::std::tr1::get<0>(GetParam());
        set_param[1] = ::std::tr1::get<1>(GetParam());
        set_param[2] = ::std::tr1::get<2>(GetParam());
        get_latitude = 0;
        get_longitude = 0;
        get_altitude = 0;
    }
    virtual void TearDown() {
        EXPECT_DOUBLE_EQ(set_param[0], get_latitude);
        EXPECT_DOUBLE_EQ(set_param[1], get_longitude);
        EXPECT_DOUBLE_EQ(set_param[2], get_altitude);
    }
protected:
    Parameters param;
    double set_param[3];
    double get_latitude;
    double get_longitude;
    double get_altitude;
};

TEST_P(camDynamicParamCoordinates, jpeg_gps_coordinates) {
    EXPECT_EQ(OK, param.setJpegGpsCoordinates(set_param));
    EXPECT_EQ(OK, param.getJpegGpsLatitude(get_latitude));
    EXPECT_EQ(OK, param.getJpegGpsLongitude(get_longitude));
    EXPECT_EQ(OK, param.getJpegGpsAltitude(get_altitude));
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamCoordinates, testing::Combine(
        testing::Values(421.2), testing::Values(123.4), testing::Values(120.33)));

class camDynamicParamAfRegions : public TestWithParam< ::std::tr1::tuple<int, int, int, int, int> > {
public:
    virtual void SetUp() {
        cur_param.bottom = ::std::tr1::get<0>(GetParam());
        cur_param.left = ::std::tr1::get<1>(GetParam());
        cur_param.right = ::std::tr1::get<2>(GetParam());
        cur_param.top = ::std::tr1::get<3>(GetParam());
        cur_param.weight = ::std::tr1::get<4>(GetParam());
        set_param.push_back(cur_param);
        set_param.push_back(cur_param);
        set_param.push_back(cur_param);
    }
    virtual void TearDown() {
        for (int i = 0; i < set_param.size(); i++) {
            EXPECT_EQ(set_param[i].bottom, get_param[i].bottom);
            EXPECT_EQ(set_param[i].left, get_param[i].left);
            EXPECT_EQ(set_param[i].right, get_param[i].right);
            EXPECT_EQ(set_param[i].top, get_param[i].top);
            EXPECT_EQ(set_param[i].weight, get_param[i].weight);
        }
    }
protected:
    Parameters param;
    camera_window_list_t set_param;
    camera_window_list_t get_param;
    camera_window_t cur_param;
};

TEST_P(camDynamicParamAfRegions, af_regions) {
    EXPECT_EQ(OK, param.setAfRegions(set_param));
    EXPECT_EQ(OK, param.getAfRegions(get_param));
    EXPECT_EQ(set_param.size(), get_param.size());
}

INSTANTIATE_TEST_CASE_P(camDynamicParam, camDynamicParamAfRegions, testing::Combine(
        testing::Values(0),
        testing::Values(0),
        testing::Values(1024),
        testing::Values(768),
        testing::Values(20)));

#endif // GTEST_HAS_PARAM_TEST

TEST(camDynamicParamChar, jpeg_gps_processing_method) {
    Parameters param;
    const uint8_t set_param[33] = {0xFF, 0xD8, 0xFF, 0xE1, 0xFF, 0xD8, 0xFF, 0xE2, 0xFF, 0xD8,
            0xFF, 0xE3, 0xFF, 0xD8, 0xFF, 0xE4, 0xFF, 0xD8, 0xFF, 0xE5, 0xFF, 0xD8, 0xFF, 0xE6,
            0xFF, 0xD8, 0xFF, 0xE7, 0xFF, 0xD8, 0xFF, 0xE8, 0};
    char get_param[33] = {};
    memset(get_param, 0, 33);
    EXPECT_EQ(OK, param.setJpegGpsProcessingMethod((const char*)set_param));
    EXPECT_EQ(OK, param.getJpegGpsProcessingMethod(32, get_param));
    EXPECT_STREQ((const char*)set_param, (const char*)get_param);
}


TEST(camHalRawTest, metadata_init_release) {
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;
    CameraMetadata mMetadata = CameraMetadata(entry_capacity,data_capacity);
    icamera_metadata_t *iCamera;

    iCamera = mMetadata.release();
    EXPECT_EQ(NULL, mMetadata.getAndLock());
    EXPECT_EQ(OK, mMetadata.unlock(NULL));

    EXPECT_NOT_NULL(iCamera);
    EXPECT_EQ(0, get_icamera_metadata_entry_count(iCamera));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(iCamera));
    EXPECT_EQ(0, get_icamera_metadata_data_count(iCamera));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(iCamera));

    FINISH_USING_CAMERA_METADATA(iCamera);
}

TEST(camHalRawTest, metadata_init_with_mbuffer_release) {
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;
    icamera_metadata_t *src = allocate_icamera_metadata(entry_capacity, data_capacity);
    icamera_metadata_t *dst;
    CameraMetadata mMetadata = CameraMetadata(src);

    dst = mMetadata.release();
    EXPECT_EQ(NULL, mMetadata.getAndLock());
    EXPECT_EQ(OK, mMetadata.unlock(NULL));

    EXPECT_NOT_NULL(dst);
    EXPECT_EQ(0, get_icamera_metadata_entry_count(dst));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(dst));
    EXPECT_EQ(0, get_icamera_metadata_data_count(dst));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(dst));

    FINISH_USING_CAMERA_METADATA(dst);
}

TEST(camHalRawTest, metadata_acquire) {
    CameraMetadata src = CameraMetadata(5,10);
    CameraMetadata dst = CameraMetadata(15,20);

    dst.acquire(src);
    icamera_metadata_t *iCamera = (icamera_metadata_t*)dst.getAndLock();

    EXPECT_NOT_NULL(iCamera);
    EXPECT_EQ(0, get_icamera_metadata_entry_count(iCamera));
    EXPECT_EQ(5, get_icamera_metadata_entry_capacity(iCamera));
    EXPECT_EQ(0, get_icamera_metadata_data_count(iCamera));
    EXPECT_EQ(10, get_icamera_metadata_data_capacity(iCamera));
    dst.unlock(iCamera);
}

TEST(camHalRawTest, metadata_update_exist) {
    CameraMetadata mCamera = CameraMetadata(5,10);
    icamera_metadata_ro_entry_t entry;
    uint32_t tag = 1;
    const string str = "test";

    EXPECT_EQ(OK, mCamera.update(tag, str));
    EXPECT_TRUE(mCamera.exists(tag));
}

TEST(camHalRawTest, metadata_append) {
    int entry_data_count = 3;
    CameraMetadata base = CameraMetadata(1, entry_data_count);
    CameraMetadata add = CameraMetadata(1, entry_data_count);
    uint8_t data[entry_data_count * 8];
    icamera_metadata_t *iCamera = NULL;

    data[0] = ICAMERA_TYPE_BYTE & 0xFF;
    data[1] = (ICAMERA_TYPE_BYTE >> 8) & 0xFF;
    data[2] = (ICAMERA_TYPE_BYTE >> 16) & 0xFF;

    iCamera = (icamera_metadata_t*)base.getAndLock();
    EXPECT_EQ(OK, add_icamera_metadata_entry(iCamera, ICAMERA_TYPE_BYTE, data, entry_data_count));
    base.unlock(iCamera);

    iCamera = (icamera_metadata_t*)add.getAndLock();
    EXPECT_EQ(OK, add_icamera_metadata_entry(iCamera, ICAMERA_TYPE_BYTE, data, entry_data_count));
    add.unlock(iCamera);

    EXPECT_EQ(OK, base.append(add));

    iCamera = (icamera_metadata_t*)base.getAndLock();
    EXPECT_NOT_NULL(iCamera);
    EXPECT_EQ(2, get_icamera_metadata_entry_count(iCamera));
    EXPECT_EQ(4, get_icamera_metadata_entry_capacity(iCamera));
    EXPECT_EQ(0, get_icamera_metadata_data_count(iCamera));
    EXPECT_EQ(3, get_icamera_metadata_data_capacity(iCamera));
    base.unlock(iCamera);

    EXPECT_EQ(OK, base.sort());
}

