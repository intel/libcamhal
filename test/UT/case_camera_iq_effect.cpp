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

#define LOG_TAG "CASE_IQ_EFFECT"

#include <algorithm>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "IspControl.h"

#include "linux/videodev2.h"
#include "iutils/Utils.h"
#include "iutils/CameraLog.h"
#include "ICamera.h"
#include "PlatformData.h"
#include "MockSysCall.h"
#include "case_common.h"
// LOCAL_TONEMAP_S
#include "AlgoTuning.h"
// LOCAL_TONEMAP_E

using namespace icamera;

// These adavance features only supported on real devices.
TEST_F(camHalTest, camhal_param_image_enhancement_saturation)
{
    if (!isFeatureSupported(IMAGE_ENHANCEMENT)) return;

    ParamList params;
    Parameters setting;
    camera_image_enhancement_t effect;
    CLEAR(effect);

    effect.saturation = 0; // Normal
    setting.setImageEnhancement(effect);
    params[0] = setting; // Effect applied at beginning

    effect.saturation = -100; // The image should tend to be B/W
    setting.setImageEnhancement(effect);
    params[4] = setting; // Effect applied at frame 4

    effect.saturation = 100; // The image should tend to be too saturated
    setting.setImageEnhancement(effect);
    params[8] = setting; // Effect applied at frame 8

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 12, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_image_enhancement_hue)
{
    if (!isFeatureSupported(IMAGE_ENHANCEMENT)) return;

    ParamList params;
    Parameters setting;
    camera_image_enhancement_t effect;
    CLEAR(effect);

    effect.hue = 0;
    setting.setImageEnhancement(effect);
    params[0] = setting; // Effect applied at beginning

    effect.hue = -100;
    setting.setImageEnhancement(effect);
    params[4] = setting; // Effect applied at frame 4

    effect.hue = 100;
    setting.setImageEnhancement(effect);
    params[8] = setting; // Effect applied at frame 8

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 12, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_image_enhancement_contrast)
{
    if (!isFeatureSupported(IMAGE_ENHANCEMENT)) return;

    ParamList params;
    Parameters setting;
    camera_image_enhancement_t effect;
    CLEAR(effect);

    effect.contrast = 0;
    setting.setImageEnhancement(effect);
    params[0] = setting; // Effect applied at beginning

    effect.contrast = -100;
    setting.setImageEnhancement(effect);
    params[4] = setting; // Effect applied at frame 4

    effect.contrast = 100;
    setting.setImageEnhancement(effect);
    params[8] = setting; // Effect applied at frame 8

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 12, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_image_enhancement_brightness)
{
    if (!isFeatureSupported(IMAGE_ENHANCEMENT)) return;

    ParamList params;
    Parameters setting;
    camera_image_enhancement_t effect;
    CLEAR(effect);

    effect.brightness = 0;
    setting.setImageEnhancement(effect);
    params[0] = setting; // Effect applied at beginning

    effect.brightness = -100;
    setting.setImageEnhancement(effect);
    params[4] = setting; // Effect applied at frame 4

    effect.brightness = 100;
    setting.setImageEnhancement(effect);
    params[8] = setting; // Effect applied at frame 8

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 12, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_image_enhancement_sharpness)
{
    if (!isFeatureSupported(IMAGE_ENHANCEMENT)) return;

    ParamList params;
    Parameters setting;
    camera_image_enhancement_t effect;
    CLEAR(effect);

    effect.sharpness = 0;
    setting.setImageEnhancement(effect);
    params[0] = setting; // Effect applied at beginning

    effect.sharpness = -100;
    setting.setImageEnhancement(effect);
    params[4] = setting; // Effect applied at frame 4

    effect.sharpness = 100;
    setting.setImageEnhancement(effect);
    params[8] = setting; // Effect applied at frame 8

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 12, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_manual_exposure)
{
    if (!isFeatureSupported(MANUAL_EXPOSURE)) return;

    ParamList params;
    Parameters setting;

    setting.setAeMode(AE_MODE_MANUAL);

    float gain = 1.0;
    int64_t exposureTime = 100;
    setting.setSensitivityGain(gain);
    setting.setExposureTime(exposureTime);
    params[0] = setting;

    gain = 1.0;
    exposureTime = 100 * 1000;
    setting.setSensitivityGain(gain);
    setting.setExposureTime(exposureTime);
    params[8] = setting;

    gain = 100.0;
    exposureTime = 100;
    setting.setSensitivityGain(gain);
    setting.setExposureTime(exposureTime);
    params[16] = setting;

    gain = 100.0;
    exposureTime = 100 * 1000;
    setting.setSensitivityGain(gain);
    setting.setExposureTime(exposureTime);
    params[24] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 32, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_exposure_compensation)
{
    if (!isFeatureSupported(MANUAL_EXPOSURE)) return;

    ParamList params;
    Parameters setting;

    setting.setAeMode(AE_MODE_AUTO);

    setting.setAeCompensation(-2);
    params[0] = setting;

    setting.setAeCompensation(0);
    params[8] = setting;

    setting.setAeCompensation(2);
    params[16] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 24, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_manual_color_transform)
{
    if (!isFeatureSupported(MANUAL_WHITE_BALANCE)) return;

    ParamList params;
    Parameters setting;

    setting.setAwbMode(AWB_MODE_MANUAL_COLOR_TRANSFORM);

    camera_color_transform_t transform;
    transform.color_transform[0][0] = 1.0;
    transform.color_transform[0][1] = -1.0;
    transform.color_transform[0][2] = 0;
    transform.color_transform[1][0] = 0;
    transform.color_transform[1][1] = 1.0;
    transform.color_transform[1][2] = -1.0;
    transform.color_transform[2][0] = 0;
    transform.color_transform[2][1] = 0;
    transform.color_transform[2][2] = 1.0;

    setting.setColorTransform(transform);
    params[4] = setting;

    setting.setAwbMode(AWB_MODE_AUTO); // Check if color can be recovered to normal
    params[10] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 16, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_manual_color_transform_gains)
{
    if (!isFeatureSupported(MANUAL_WHITE_BALANCE)) return;

    ParamList params;
    Parameters setting;

    setting.setAwbMode(AWB_MODE_MANUAL_COLOR_TRANSFORM);

    camera_color_transform_t transform;
    transform.color_transform[0][0] = 1.0;
    transform.color_transform[0][1] = 0;
    transform.color_transform[0][2] = 0;
    transform.color_transform[1][0] = 0;
    transform.color_transform[1][1] = 1.0;
    transform.color_transform[1][2] = 0.0;
    transform.color_transform[2][0] = 0;
    transform.color_transform[2][1] = 0;
    transform.color_transform[2][2] = 1.0;

    camera_color_gains_t gains;
    gains.color_gains_rggb[0] = 1;
    gains.color_gains_rggb[1] = 1;
    gains.color_gains_rggb[2] = 1;
    gains.color_gains_rggb[3] = 6;  // boost blue

    setting.setColorTransform(transform);
    setting.setColorGains(gains);
    params[4] = setting;

    setting.setAwbMode(AWB_MODE_AUTO); // Check if color can be recovered to normal
    params[10] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 16, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_manual_white_balance)
{
    if (!isFeatureSupported(MANUAL_WHITE_BALANCE)) return;

    ParamList params;
    Parameters setting;

    setting.setAwbMode(AWB_MODE_AUTO);
    params[0] = setting;

    setting.setAwbMode(AWB_MODE_FLUORESCENT);
    params[6] = setting;

    setting.setAwbMode(AWB_MODE_SUNSET);
    params[12] = setting;

    camera_range_t cct_range = {9000, 10000};
    setting.setAwbMode(AWB_MODE_MANUAL_CCT_RANGE);
    setting.setAwbCctRange(cct_range);
    params[18] = setting;

    camera_coordinate_t white_point = {100, 100};
    setting.setAwbMode(AWB_MODE_MANUAL_WHITE_POINT);
    setting.setAwbWhitePoint(white_point);
    params[24] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 30, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_manual_awb_manual_gain)
{
    if (!isFeatureSupported(MANUAL_WHITE_BALANCE)) return;

    ParamList params;
    Parameters setting;

    setting.setAwbMode(AWB_MODE_AUTO);
    params[0] = setting;

    setting.setAwbMode(AWB_MODE_MANUAL_GAIN);
    setting.setAwbGains({200, 0, 0});
    params[10] = setting;

    setting.setAwbMode(AWB_MODE_MANUAL_GAIN);
    setting.setAwbGains({0, 200, 0});
    params[20] = setting;

    setting.setAwbMode(AWB_MODE_MANUAL_GAIN);
    setting.setAwbGains({0, 0, 200});
    params[30] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 40, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_set_ae_converge_speed)
{
    if (!isFeatureSupported(MANUAL_EXPOSURE)) return;

    ParamList params;
    Parameters setting;

    setting.setAeConvergeSpeed(CONVERGE_LOW);
    params[10] = setting;

    setting.setAeConvergeSpeed(CONVERGE_NORMAL);
    params[30] = setting;

    setting.setAeConvergeSpeed(CONVERGE_MID);
    params[50] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 80, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_set_awb_converge_speed)
{
    if (!isFeatureSupported(MANUAL_WHITE_BALANCE)) return;

    ParamList params;
    Parameters setting;

    setting.setAwbConvergeSpeed(CONVERGE_LOW);
    params[10] = setting;

    setting.setAwbConvergeSpeed(CONVERGE_NORMAL);
    params[30] = setting;

    setting.setAwbConvergeSpeed(CONVERGE_MID);
    params[50] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 80, V4L2_FIELD_ANY, &params);
}

/*
//Not enabled until FW add new PGs for new normal pipe.
TEST_F(camHalTest, camhal_param_scene_mode_normal)
{
    if (!isFeatureSupported(SCENE_MODE)) return;
    if (!PlatformData::isEnableHDR(getCurrentCameraId())) return;

    ParamList params;
    Parameters setting;

    setting.setSceneMode(SCENE_MODE_NORMAL);
    params[0] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 24, V4L2_FIELD_ANY, &params);
}
*/

TEST_F(camHalTest, camhal_param_scene_mode_hdr)
{
    if (!isFeatureSupported(SCENE_MODE)) return;
    if (!PlatformData::isEnableHDR(getCurrentCameraId())) return;

    ParamList params;
    Parameters setting;

    setting.setSceneMode(SCENE_MODE_HDR);
    params[0] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 24, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_scene_mode_ull)
{
    if (!isFeatureSupported(SCENE_MODE)) return;
    if (!PlatformData::isEnableHDR(getCurrentCameraId())) return;

    ParamList params;
    Parameters setting;

    setting.setSceneMode(SCENE_MODE_ULL);
    params[0] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 24, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_scene_mode_video_ll)
{
    if (!isFeatureSupported(SCENE_MODE)) return;
    if (!PlatformData::isEnableHDR(getCurrentCameraId())) return;

    ParamList params;
    Parameters setting;

    setting.setSceneMode(SCENE_MODE_VIDEO_LL);
    params[0] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 24, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_switch_scene_mode)
{
    if (!isFeatureSupported(SCENE_MODE)) return;

    if (!PlatformData::isEnableHDR(getCurrentCameraId())) return;

    ParamList params;
    Parameters setting;

    setting.setSceneMode(SCENE_MODE_HDR);
    params[0] = setting;

    setting.setSceneMode(SCENE_MODE_ULL);
    params[20] = setting;

    setting.setSceneMode(SCENE_MODE_AUTO);
    params[40] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 60, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_isp_control_csc)
{
    camera_control_isp_ctrl_id cscCtrlId = camera_control_isp_ctrl_id_bxt_csc;

    if (!isFeatureSupported(ISP_CONTROL) ||
        !PlatformData::isIspControlFeatureSupported(getCurrentCameraId(), cscCtrlId)) {
        return;
    }

    ParamList params;
    Parameters setting;
    setting.setEnabledIspControls({cscCtrlId});

    camera_control_isp_bxt_csc_t csc1 = {3483, 11718, 1183, 1877, -6315, 8192, 8192, -7441, -751};
    setting.setIspControl(cscCtrlId, &csc1);
    params[20] = setting;

    camera_control_isp_bxt_csc_t csc2 = {-3483, -11718, 1183, -1877, -6315, 8192, 8192, 7441, 751};
    setting.setIspControl(cscCtrlId, &csc2);
    params[40] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 60, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_isp_control_ccm)
{
    camera_control_isp_ctrl_id ccmCtrlId = camera_control_isp_ctrl_id_color_correction_matrix;
    if (!isFeatureSupported(ISP_CONTROL) ||
        !PlatformData::isIspControlFeatureSupported(getCurrentCameraId(), ccmCtrlId)) {
        return;
    }

    ParamList params;
    Parameters setting;
    setting.setEnabledIspControls({ccmCtrlId});

    camera_control_isp_color_correction_matrix_t ccm1 = {0, 0, 1, 0, 1, 0, 1, 0, 0};
    setting.setIspControl(ccmCtrlId, &ccm1);
    params[20] = setting;

    camera_control_isp_color_correction_matrix_t ccm2 = {-1, 0, 2, 1, 0, -1, 1, 2, 0};
    setting.setIspControl(ccmCtrlId, &ccm2);
    params[40] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 60, V4L2_FIELD_ANY, &params);
}

TEST_F(camHalTest, camhal_param_isp_control_wb_gain)
{
    camera_control_isp_ctrl_id wbGainCtrlId = camera_control_isp_ctrl_id_wb_gains;
    if (!isFeatureSupported(ISP_CONTROL) ||
        !PlatformData::isIspControlFeatureSupported(getCurrentCameraId(), wbGainCtrlId)) {
        return;
    }

    ParamList params;
    Parameters setting;
    setting.setEnabledIspControls({wbGainCtrlId});

    camera_control_isp_wb_gains_t wbGain1 = {2.0, 4.0, 4.0, 2.0};
    setting.setIspControl(wbGainCtrlId, &wbGain1);
    params[20] = setting;

    camera_control_isp_wb_gains_t wbGain2 = {4.0, 2.0, 2.0, 4.0};
    setting.setIspControl(wbGainCtrlId, &wbGain2);
    params[40] = setting;

    setting.setIspControl(wbGainCtrlId, NULL);
    params[60] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 80, V4L2_FIELD_ANY, &params);
}

// LOCAL_TONEMAP_S
TEST_F(camHalTest, camhal_param_ltm_tuning)
{
    ParamList params;
    Parameters setting;
    ltm_tuning_data ltmTuning;
    CLEAR(ltmTuning);

    ltmTuning.algo_mode = ltm_algo_optibright_gain_map;

    ltmTuning.optibright_tuning.GTM_Str = 120;
    ltmTuning.optibright_tuning.GF_epspar = 2621;
    ltmTuning.optibright_tuning.alpham1 = 32767;
    ltmTuning.optibright_tuning.alpham = 21299;
    ltmTuning.optibright_tuning.maskmin = 0;
    ltmTuning.optibright_tuning.maskmax = 4915;
    ltmTuning.optibright_tuning.num_iteration = 16;
    ltmTuning.optibright_tuning.maskmid = 4915;
    ltmTuning.optibright_tuning.hlc_mode = 0;
    ltmTuning.optibright_tuning.max_isp_gain = 32;
    ltmTuning.optibright_tuning.convergence_speed = 1229;
    ltmTuning.optibright_tuning.lm_treatment = 22938;
    ltmTuning.optibright_tuning.GTM_mode = 1;
    ltmTuning.optibright_tuning.pre_gamma = 60;
    ltmTuning.optibright_tuning.lav2p_scale = 5;
    ltmTuning.optibright_tuning.p_max = 9830;
    ltmTuning.optibright_tuning.p_mode = 0;
    ltmTuning.optibright_tuning.p_value = 9830;
    ltmTuning.optibright_tuning.filter_size = 0;
    ltmTuning.optibright_tuning.max_percentile = 32604;
    ltmTuning.optibright_tuning.ldr_brightness = 10650;
    ltmTuning.optibright_tuning.dr_mid = 7022;
    ltmTuning.optibright_tuning.dr_norm_max = 7168;
    ltmTuning.optibright_tuning.dr_norm_min = 0;
    ltmTuning.optibright_tuning.convergence_speed_slow = 8192;
    ltmTuning.optibright_tuning.convergence_sigma = 4915;
    ltmTuning.optibright_tuning.gainext_mode = 1;
    ltmTuning.optibright_tuning.wdr_scale_max = 12288;
    ltmTuning.optibright_tuning.wdr_scale_min = 1024;
    ltmTuning.optibright_tuning.wdr_gain_max = 16384;
    ltmTuning.optibright_tuning.frame_delay_compensation = 1;

    ltmTuning.mpgc_tuning.lm_stability = 3277;
    ltmTuning.mpgc_tuning.lm_sensitivity = 16;
    ltmTuning.mpgc_tuning.blur_size = 1;
    ltmTuning.mpgc_tuning.tf_str = 6553;

    ltmTuning.drcsw_tuning.blus_sim_sigma = 8192;

    setting.setLtmTuningData(&ltmTuning);
    params[20] = setting;
    params[60] = setting;

    ltmTuning.optibright_tuning.wdr_scale_max = 2000;
    ltmTuning.optibright_tuning.wdr_scale_min = 1024;
    ltmTuning.optibright_tuning.wdr_gain_max = 16;
    setting.setLtmTuningData(&ltmTuning);
    params[40] = setting;
    params[80] = setting;

    camhal_qbuf_dqbuf_common(1920, 1080, V4L2_PIX_FMT_NV12, 8, 100, V4L2_FIELD_ANY, &params);
}
// LOCAL_TONEMAP_E

TEST_F(camHalTest, camhal_param_isp_control_get_set_wb_gain)
{
    camera_control_isp_ctrl_id wbGainCtrlId = camera_control_isp_ctrl_id_wb_gains;

    if (!isFeatureSupported(ISP_CONTROL) ||
        !PlatformData::isIspControlFeatureSupported(getCurrentCameraId(), wbGainCtrlId)) {
        return;
    }

    camera_hal_init();

    int cameraId = getCurrentCameraId();

    vector<uint32_t> controls = PlatformData::getSupportedIspControlFeatures(cameraId);
    bool isWbGainSupported = false;
    for (auto ctrlId : controls) {
        if (ctrlId == wbGainCtrlId) {
            isWbGainSupported = true;
            break;
        }
    }
    if (!isWbGainSupported) {
        camera_hal_deinit();
        return;
    }

    camera_info_t info;
    get_camera_info(cameraId, info);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);

    camera_device_open(cameraId);

    // Configure streams
    int stream_id = -1;
    stream_config_t stream_list;
    stream_t streams[1];
    streams[0] = getStreamByConfig(configs[0]);
    streams[0].memType = V4L2_MEMORY_USERPTR;
    stream_list.num_streams = 1;
    stream_list.streams = streams;
    stream_list.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;

    camera_device_config_streams(cameraId, &stream_list);

    camera_control_isp_wb_gains_t wbGainGetDef = {0, 0, 0, 0};
    Parameters paramGetDef;
    camera_get_parameters(cameraId, paramGetDef);
    // Get the default value for wb_gains.
    paramGetDef.getIspControl(wbGainCtrlId, &wbGainGetDef);
    EXPECT_NE(wbGainGetDef.r, 0);
    EXPECT_NE(wbGainGetDef.gr, 0);
    EXPECT_NE(wbGainGetDef.gb, 0);
    EXPECT_NE(wbGainGetDef.b, 0);

    Parameters paramSet;
    camera_control_isp_wb_gains_t wbGainSet = {1.0, 2.0, 3.0, 4.0};
    paramSet.setIspControl(wbGainCtrlId, &wbGainSet);
    paramSet.setEnabledIspControls({wbGainCtrlId});
    camera_set_parameters(cameraId, paramSet);

    Parameters paramGet;
    camera_get_parameters(cameraId, paramGet);

    camera_control_isp_wb_gains_t wbGainGet;
    CLEAR(wbGainGet);
    paramGet.getIspControl(wbGainCtrlId, &wbGainGet);
    EXPECT_EQ(wbGainSet.b, wbGainGet.b);
    EXPECT_EQ(wbGainSet.r, wbGainGet.r);
    EXPECT_EQ(wbGainSet.gb, wbGainGet.gb);
    EXPECT_EQ(wbGainSet.gr, wbGainGet.gr);

    camera_device_close(cameraId);
    camera_hal_deinit();
}

