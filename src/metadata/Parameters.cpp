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

#define LOG_TAG "Parameters"

#include "iutils/Errors.h"
#include "iutils/CameraLog.h"
#include "iutils/Utils.h"

#include "PlatformData.h"
#include "Parameters.h"
#include "ParameterHelper.h"

#include "IspControl.h"
#include "isp_control/IspControlUtils.h"

namespace icamera {

Parameters::Parameters() : mData(ParameterHelper::createParameterData()) {}

Parameters::Parameters(const Parameters& other) :
        mData(ParameterHelper::createParameterData(other.mData)) {}

Parameters& Parameters::operator=(const Parameters& other)
{
    ParameterHelper::AutoWLock wl(mData);
    ParameterHelper::deepCopy(other.mData, mData);
    return *this;
}

Parameters::~Parameters()
{
    ParameterHelper::releaseParameterData(mData);
    mData = nullptr;
}

void Parameters::merge(const Parameters& other)
{
    ParameterHelper::merge(other, this);
}

int Parameters::setAeMode(camera_ae_mode_t aeMode)
{
    uint8_t mode = aeMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AE_MODE, &mode, 1);
}

int Parameters::getAeMode(camera_ae_mode_t& aeMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    aeMode = (camera_ae_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAeState(camera_ae_state_t aeState)
{
    uint8_t state = aeState;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AE_STATE, &state, 1);
}

int Parameters::getAeState(camera_ae_state_t& aeState) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_STATE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    aeState = (camera_ae_state_t)entry.data.u8[0];
    return OK;
}

static int setRegions(CameraMetadata& metadata, camera_window_list_t regions, int tag)
{
    if (regions.empty()) {
        // Nothing to do with an empty parameter.
        return INVALID_OPERATION;
    }

    const int ELEM_NUM = sizeof(camera_window_t) / sizeof(int);
    int values[regions.size() * ELEM_NUM];
    for (size_t i = 0; i < regions.size(); i++) {
        values[i * ELEM_NUM] = regions[i].left;
        values[i * ELEM_NUM + 1] = regions[i].top;
        values[i * ELEM_NUM + 2] = regions[i].right;
        values[i * ELEM_NUM + 3] = regions[i].bottom;
        values[i * ELEM_NUM + 4] = regions[i].weight;
    }

    return metadata.update(tag, values, ARRAY_SIZE(values));
}

static int getRegions(icamera_metadata_ro_entry_t entry, camera_window_list_t& regions)
{
    regions.clear();
    const int ELEM_NUM = sizeof(camera_window_t) / sizeof(int);
    if (entry.count == 0 || entry.count % ELEM_NUM != 0) {
        return NAME_NOT_FOUND;
    }

    camera_window_t w;
    for (size_t i = 0; i < entry.count; i += ELEM_NUM) {
        w.left = entry.data.i32[i];
        w.top = entry.data.i32[i + 1];
        w.right = entry.data.i32[i + 2];
        w.bottom = entry.data.i32[i + 3];
        w.weight = entry.data.i32[i + 4];
        regions.push_back(w);
    }

    return OK;
}

int Parameters::setAeRegions(camera_window_list_t aeRegions)
{
    ParameterHelper::AutoWLock wl(mData);
    return setRegions(ParameterHelper::getMetadata(mData), aeRegions, CAMERA_AE_REGIONS);
}

int Parameters::getAeRegions(camera_window_list_t& aeRegions) const
{
    ParameterHelper::AutoRLock rl(mData);
    return getRegions(ParameterHelper::getMetadataEntry(mData, CAMERA_AE_REGIONS), aeRegions);
}

int Parameters::setAeLock(bool lock)
{
    uint8_t lockValue = lock ? 1 : 0;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AE_LOCK, &lockValue, 1);
}

int Parameters::getAeLock(bool& lock) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_LOCK);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    lock = entry.data.u8[0];
    return OK;
}

int Parameters::setExposureTime(int64_t exposureTime)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_SENSOR_EXPOSURE_TIME, &exposureTime, 1);
}

int Parameters::getExposureTime(int64_t& exposureTime) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_SENSOR_EXPOSURE_TIME);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    exposureTime = entry.data.i64[0];
    return OK;
}

int Parameters::setSensitivityGain(float gain)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_SENSITIVITY_GAIN, &gain, 1);
}

int Parameters::getSensitivityGain(float& gain) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_SENSITIVITY_GAIN);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    gain = entry.data.f[0];
    return OK;
}

int Parameters::setAeCompensation(int ev)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AE_COMPENSATION, &ev, 1);
}

int Parameters::getAeCompensation(int& ev) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_COMPENSATION);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    ev = entry.data.i32[0];
    return OK;
}

int Parameters::setFrameRate(float fps)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_FRAME_RATE, &fps, 1);
}

int Parameters::getFrameRate(float& fps) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_FRAME_RATE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    fps = entry.data.f[0];
    return OK;
}

int Parameters::setAntiBandingMode(camera_antibanding_mode_t bandingMode)
{
    uint8_t mode = bandingMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AE_ANTIBANDING_MODE, &mode, 1);
}

int Parameters::getAntiBandingMode(camera_antibanding_mode_t& bandingMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_ANTIBANDING_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    bandingMode = (camera_antibanding_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAwbMode(camera_awb_mode_t awbMode)
{
    uint8_t mode = awbMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_MODE, &mode, 1);
}

int Parameters::getAwbMode(camera_awb_mode_t& awbMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    awbMode = (camera_awb_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAwbState(camera_awb_state_t awbState)
{
    uint8_t state = awbState;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_STATE, &state, 1);
}

int Parameters::getAwbState(camera_awb_state_t& awbState) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_STATE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    awbState = (camera_awb_state_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAwbLock(bool lock)
{
    uint8_t lockValue = lock ? 1 : 0;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_LOCK, &lockValue, 1);
}

int Parameters::getAwbLock(bool& lock) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_LOCK);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    lock = entry.data.u8[0];
    return OK;
}

int Parameters::setAwbCctRange(camera_range_t cct)
{
    int range[] = {(int)cct.min, (int)cct.max};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_CCT_RANGE, range, ARRAY_SIZE(range));
}

int Parameters::getAwbCctRange(camera_range_t& cct) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_CCT_RANGE);
    const size_t ELEM_NUM = sizeof(camera_range_t) / sizeof(int);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    cct.min = entry.data.i32[0];
    cct.max = entry.data.i32[1];
    return OK;
}

int Parameters::setAwbGains(camera_awb_gains_t awbGains)
{
    int values[] = {awbGains.r_gain, awbGains.g_gain, awbGains.b_gain};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_GAINS, values, ARRAY_SIZE(values));
}

int Parameters::getAwbGains(camera_awb_gains_t& awbGains) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_GAINS);
    const size_t ELEM_NUM = sizeof(camera_awb_gains_t) / sizeof(int);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    awbGains.r_gain = entry.data.i32[0];
    awbGains.g_gain = entry.data.i32[1];
    awbGains.b_gain = entry.data.i32[2];
    return OK;
}

int Parameters::setAwbResult(void* data)
{
    uint32_t size = sizeof(camera_awb_result_t);
    uint32_t tag = CAMERA_AWB_RESULT;
    ParameterHelper::AutoWLock wl(mData);

    if (data == NULL) {
        return ParameterHelper::getMetadata(mData).erase(tag);
    }
    return ParameterHelper::getMetadata(mData).update(tag, (uint8_t*)data, size);
}

int Parameters::getAwbResult(void* data) const
{
    if (data == NULL) {
        return BAD_VALUE;
    }

    uint32_t size = sizeof(camera_awb_result_t);
    uint32_t tag = CAMERA_AWB_RESULT;
    ParameterHelper::AutoRLock rl(mData);

    auto entry = ParameterHelper::getMetadataEntry(mData, tag);
    if (entry.count != size) {
        return NAME_NOT_FOUND;
    }

    MEMCPY_S(data, size, entry.data.u8, size);

    return OK;
}


int Parameters::setAwbGainShift(camera_awb_gains_t awbGainShift)
{
    int values[] = {awbGainShift.r_gain, awbGainShift.g_gain, awbGainShift.b_gain};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_GAIN_SHIFT, values, ARRAY_SIZE(values));
}

int Parameters::getAwbGainShift(camera_awb_gains_t& awbGainShift) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_GAIN_SHIFT);
    const size_t ELEM_NUM = sizeof(camera_awb_gains_t) / sizeof(int);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    awbGainShift.r_gain = entry.data.i32[0];
    awbGainShift.g_gain = entry.data.i32[1];
    awbGainShift.b_gain = entry.data.i32[2];
    return OK;
}

int Parameters::setAwbWhitePoint(camera_coordinate_t whitePoint)
{
    int values[] = {whitePoint.x, whitePoint.y};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_WHITE_POINT, values, ARRAY_SIZE(values));
}

int Parameters::getAwbWhitePoint(camera_coordinate_t& whitePoint) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_WHITE_POINT);
    const size_t ELEM_NUM = sizeof(camera_coordinate_t) / sizeof(int);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }

    whitePoint.x = entry.data.i32[0];
    whitePoint.y = entry.data.i32[1];

    return OK;
}

int Parameters::setColorTransform(camera_color_transform_t colorTransform)
{
    float* transform = (float*)colorTransform.color_transform;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_COLOR_TRANSFORM, transform, 3 * 3);
}

int Parameters::getColorTransform(camera_color_transform_t& colorTransform) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_COLOR_TRANSFORM);
    const size_t ELEM_NUM = 3 * 3;
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    for (size_t i = 0; i < ELEM_NUM; i++) {
        colorTransform.color_transform[i / 3][i % 3] = entry.data.f[i];
    }

    return OK;
}

int Parameters::setColorGains(camera_color_gains_t colorGains)
{
    float* gains = colorGains.color_gains_rggb;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_COLOR_GAINS, gains, 4);
}

int Parameters::getColorGains(camera_color_gains_t& colorGains) const
{
    ParameterHelper::AutoRLock rl(mData);
    icamera_metadata_ro_entry_t entry =
        ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_COLOR_GAINS);
    const size_t ELEM_NUM = 4;
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    for (size_t i = 0; i < ELEM_NUM; i++) {
        colorGains.color_gains_rggb[i] = entry.data.f[i];
    }
    return OK;
}

int Parameters::setAwbRegions(camera_window_list_t awbRegions)
{
    ParameterHelper::AutoWLock wl(mData);
    return setRegions(ParameterHelper::getMetadata(mData), awbRegions, CAMERA_AWB_REGIONS);
}

int Parameters::getAwbRegions(camera_window_list_t& awbRegions) const
{
    ParameterHelper::AutoRLock rl(mData);
    return getRegions(ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_REGIONS), awbRegions);
}

int Parameters::setNrMode(camera_nr_mode_t nrMode)
{
    uint8_t mode = nrMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_NR_MODE, &mode, 1);
}

int Parameters::getNrMode(camera_nr_mode_t& nrMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_NR_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    nrMode = (camera_nr_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setNrLevel(camera_nr_level_t level)
{
    int values [] = {level.overall, level.spatial, level.temporal};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_NR_LEVEL, values, ARRAY_SIZE(values));
}

int Parameters::getNrLevel(camera_nr_level_t& level) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_NR_LEVEL);
    const size_t ELEM_NUM = sizeof(camera_nr_level_t) / sizeof(int);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    level.overall = entry.data.i32[0];
    level.spatial = entry.data.i32[1];
    level.temporal = entry.data.i32[2];
    return OK;
}

int Parameters::setIrisMode(camera_iris_mode_t irisMode)
{
    uint8_t mode = irisMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_IRIS_MODE, &mode, 1);
}

int Parameters::getIrisMode(camera_iris_mode_t& irisMode)
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_IRIS_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    irisMode = (camera_iris_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setIrisLevel(int level)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_IRIS_LEVEL, &level, 1);
}

int Parameters::getIrisLevel(int& level)
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_IRIS_LEVEL);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    level = entry.data.i32[0];
    return OK;
}

int Parameters::setWdrMode(camera_wdr_mode_t wdrMode)
{
    uint8_t mode = wdrMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_WDR_MODE, &mode, 1);
}

int Parameters::getWdrMode(camera_wdr_mode_t& wdrMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_WDR_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    wdrMode = (camera_wdr_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setWdrLevel(uint8_t level)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_WDR_LEVEL, &level, 1);
}

int Parameters::getWdrLevel(uint8_t& level) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_WDR_LEVEL);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    level = entry.data.u8[0];
    return OK;
}

int Parameters::setEffectSceneMode(camera_scene_mode_t sceneMode)
{
    uint8_t mode = sceneMode;
    LOGW("Effect scene mode is deprecated. Please use setSceneMode() instead.");
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_SCENE_MODE, &mode, 1);
}

int Parameters::getEffectSceneMode(camera_scene_mode_t& sceneMode) const
{
    LOGW("Effect scene mode is deprecated. Please use getSceneMode() instead.");
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_SCENE_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    sceneMode = (camera_scene_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setSceneMode(camera_scene_mode_t sceneMode)
{
    uint8_t mode = sceneMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_SCENE_MODE, &mode, 1);
}

int Parameters::getSceneMode(camera_scene_mode_t& sceneMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_SCENE_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    sceneMode = (camera_scene_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setWeightGridMode(camera_weight_grid_mode_t weightGridMode)
{
    uint8_t mode = (uint8_t)weightGridMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_WEIGHT_GRID_MODE, &mode, 1);
}

int Parameters::getWeightGridMode(camera_weight_grid_mode_t& weightGridMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_WEIGHT_GRID_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    weightGridMode = (camera_weight_grid_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setBlcAreaMode(camera_blc_area_mode_t blcAreaMode)
{
    uint8_t mode = blcAreaMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_BLC_AREA_MODE, &mode, 1);
}

int Parameters::getBlcAreaMode(camera_blc_area_mode_t& blcAreaMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_BLC_AREA_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    blcAreaMode = (camera_blc_area_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setFpsRange(camera_range_t fpsRange)
{
    float range[] = {fpsRange.min, fpsRange.max};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AE_TARGET_FPS_RANGE, range, ARRAY_SIZE(range));
}

int Parameters::getFpsRange(camera_range_t& fpsRange) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_TARGET_FPS_RANGE);
    const size_t ELEM_NUM = sizeof(camera_range_t) / sizeof(float);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }
    fpsRange.min = entry.data.f[0];
    fpsRange.max = entry.data.f[1];
    return OK;
}

int Parameters::setImageEnhancement(camera_image_enhancement_t effects)
{
    int values[] = {effects.sharpness, effects.brightness, effects.contrast, effects.hue, effects.saturation};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_IMAGE_ENHANCEMENT, values, ARRAY_SIZE(values));
}

int Parameters::getImageEnhancement(camera_image_enhancement_t& effects) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_IMAGE_ENHANCEMENT);
    size_t number_of_effects = sizeof(camera_image_enhancement_t) / sizeof(int);
    if (entry.count != number_of_effects) {
        return NAME_NOT_FOUND;
    }
    effects.sharpness = entry.data.i32[0];
    effects.brightness = entry.data.i32[1];
    effects.contrast = entry.data.i32[2];
    effects.hue = entry.data.i32[3];
    effects.saturation = entry.data.i32[4];

    return OK;
}

int Parameters::setDeinterlaceMode(camera_deinterlace_mode_t deinterlaceMode)
{
    uint8_t mode = deinterlaceMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_DEINTERLACE_MODE, &mode, 1);
}

int Parameters::getDeinterlaceMode(camera_deinterlace_mode_t &deinterlaceMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_DEINTERLACE_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    deinterlaceMode = (camera_deinterlace_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::getSupportedVideoStabilizationMode(camera_video_stabilization_list_t &supportedModes) const
{
    supportedModes.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_CONTROL_AVAILABLE_VIDEO_STABILIZATION_MODES);
    for (size_t i = 0; i < entry.count; i++) {
        supportedModes.push_back((camera_video_stabilization_mode_t)entry.data.u8[i]);
    }

    return OK;
}

int Parameters::getSupportedAeMode(vector <camera_ae_mode_t> &supportedAeModes) const
{
    supportedAeModes.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_AVAILABLE_MODES);
    for (size_t i = 0; i < entry.count; i++) {
        supportedAeModes.push_back((camera_ae_mode_t)entry.data.u8[i]);
    }

    return OK;
}

int Parameters::getSupportedAwbMode(vector <camera_awb_mode_t> &supportedAwbModes) const
{
    supportedAwbModes.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_AVAILABLE_MODES);
    for (size_t i = 0; i < entry.count; i++) {
        supportedAwbModes.push_back((camera_awb_mode_t)entry.data.u8[i]);
    }

    return OK;
}

int Parameters::getSupportedAfMode(vector <camera_af_mode_t> &supportedAfModes) const
{
    supportedAfModes.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AF_AVAILABLE_MODES);
    for (size_t i = 0; i < entry.count; i++) {
        supportedAfModes.push_back((camera_af_mode_t)entry.data.u8[i]);
    }

    return OK;
}

int Parameters::getSupportedSceneMode(vector <camera_scene_mode_t> &supportedSceneModes) const
{
    supportedSceneModes.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_CONTROL_AVAILABLE_SCENE_MODES);
    for (size_t i = 0; i < entry.count; i++) {
        supportedSceneModes.push_back((camera_scene_mode_t)entry.data.u8[i]);
    }

    return OK;
}

int Parameters::getSupportedAntibandingMode(vector <camera_antibanding_mode_t> &supportedAntibindingModes) const
{
    supportedAntibindingModes.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_AVAILABLE_ANTIBANDING_MODES);
    for (size_t i = 0; i < entry.count; i++) {
        supportedAntibindingModes.push_back((camera_antibanding_mode_t)entry.data.u8[i]);
    }

    return OK;
}

int Parameters::getSupportedFpsRange(camera_range_array_t& ranges) const
{
    ranges.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_AVAILABLE_TARGET_FPS_RANGES);
    if (entry.count == 0 || entry.count % 2 != 0) {
        return NAME_NOT_FOUND;
    }

    camera_range_t fps;
    for (size_t i = 0; i < entry.count; i += 2) {
        fps.min = entry.data.f[i];
        fps.max = entry.data.f[i + 1];
        ranges.push_back(fps);
    }

    return OK;
}

int Parameters::getSupportedStreamConfig(supported_stream_config_array_t& config) const
{
    config.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_SCALER_AVAILABLE_STREAM_CONFIGURATIONS);
    const int streamConfMemberNum = sizeof(supported_stream_config_t) / sizeof(int);
    if (entry.count == 0 || entry.count % streamConfMemberNum != 0) {
        return NAME_NOT_FOUND;
    }

    supported_stream_config_t cfg;
    CLEAR(cfg);

    for (size_t i = 0; i < entry.count; i += streamConfMemberNum) {
        MEMCPY_S(&cfg, sizeof(supported_stream_config_t), &entry.data.i32[i], sizeof(supported_stream_config_t));
        cfg.stride = CameraUtils::getStride(cfg.format, cfg.width);
        cfg.size   = CameraUtils::getFrameSize(cfg.format, cfg.width, cfg.height);
        config.push_back(cfg);
    }
    return OK;
}

int Parameters::getSupportedSensorExposureTimeRange(camera_range_t& range) const
{
    CLEAR(range);
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_SENSOR_INFO_EXPOSURE_TIME_RANGE);
    if (entry.count != 2) {
        return NAME_NOT_FOUND;
    }

    range.min = (float)(entry.data.i64[0]);
    range.max = (float)(entry.data.i64[1]);
    return OK;
}

int Parameters::getSupportedSensorSensitivityRange(camera_range_t& range) const
{
    CLEAR(range);
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_SENSOR_INFO_SENSITIVITY_RANGE);
    if (entry.count != 2) {
        return NAME_NOT_FOUND;
    }

    range.min = entry.data.i32[0];
    range.max = entry.data.i32[1];
    return OK;
}

int Parameters::getSupportedFeatures(camera_features_list_t& features) const
{
    features.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_INFO_AVAILABLE_FEATURES);
    for (size_t i = 0; i < entry.count; i++) {
        features.push_back((camera_features)entry.data.u8[i]);
    }
    return OK;
}

int Parameters::getSupportedIspControlFeatures(vector<uint32_t>& controls) const
{
    controls.clear();
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_ISP_SUPPORTED_CTRL_IDS);
    for (size_t i = 0; i < entry.count; i++) {
        controls.push_back((uint32_t)entry.data.i32[i]);
    }

    return OK;
}

int Parameters::getAeCompensationRange(camera_range_t& evRange) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_COMPENSATION_RANGE);
    const size_t ELEM_NUM = sizeof(camera_range_t) / sizeof(int);
    if (entry.count != ELEM_NUM) {
        return NAME_NOT_FOUND;
    }

    evRange.min = entry.data.i32[0];
    evRange.max = entry.data.i32[1];
    return OK;
}

int Parameters::getAeCompensationStep(camera_rational_t& evStep) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_COMPENSATION_STEP);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    evStep.numerator = entry.data.r[0].numerator;
    evStep.denominator = entry.data.r[0].denominator;
    return OK;
}

int Parameters::getSupportedAeExposureTimeRange(std::vector<camera_ae_exposure_time_range_t> & etRanges) const
{
    ParameterHelper::AutoRLock rl(mData);

    const int MEMBER_COUNT = 3;
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_INFO_AE_EXPOSURE_TIME_RANGE);
    if (entry.count == 0 || entry.count % MEMBER_COUNT != 0) {
        return NAME_NOT_FOUND;
    }

    camera_ae_exposure_time_range_t range;
    CLEAR(range);

    for (size_t i = 0; i < entry.count; i += MEMBER_COUNT) {
        range.scene_mode = (camera_scene_mode_t)entry.data.i32[i];
        range.et_range.min = entry.data.i32[i + 1];
        range.et_range.max = entry.data.i32[i + 2];
        etRanges.push_back(range);
    }
    return OK;
}

int Parameters::getSupportedAeGainRange(std::vector<camera_ae_gain_range_t>& gainRanges) const
{
    ParameterHelper::AutoRLock rl(mData);

    const int MEMBER_COUNT = 3;
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_INFO_AE_GAIN_RANGE);
    if (entry.count == 0 || entry.count % MEMBER_COUNT != 0) {
        return NAME_NOT_FOUND;
    }

    camera_ae_gain_range_t range;
    CLEAR(range);

    for (size_t i = 0; i < entry.count; i += MEMBER_COUNT) {
        range.scene_mode = (camera_scene_mode_t)entry.data.i32[i];
        // Since we use int to store float, before storing it we multiply min and max by 100,
        // so we need to divide 100 when giving them outside.
        range.gain_range.min = (float)entry.data.i32[i + 1] / 100.0;
        range.gain_range.max = (float)entry.data.i32[i + 2] / 100.0;
        gainRanges.push_back(range);
    }
    return OK;
}

bool Parameters::getAeLockAvailable() const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AE_LOCK_AVAILABLE);
    if (entry.count != 1) {
        return false;
    }

    return (entry.data.u8[0] == CAMERA_AE_LOCK_AVAILABLE_TRUE);
}

bool Parameters::getAwbLockAvailable() const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_LOCK_AVAILABLE);
    if (entry.count != 1) {
        return false;
    }

    return (entry.data.u8[0] == CAMERA_AWB_LOCK_AVAILABLE_TRUE);
}

int Parameters::setExposureTimeRange(camera_range_t exposureTimeRange)
{
    ParameterHelper::AutoWLock wl(mData);
    const int MEMBER_COUNT = 2;
    int values[MEMBER_COUNT] = {(int)exposureTimeRange.min, (int)exposureTimeRange.max};
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_EXPOSURE_TIME_RANGE, values, MEMBER_COUNT);
}

int Parameters::getExposureTimeRange(camera_range_t& exposureTimeRange) const
{
    ParameterHelper::AutoRLock rl(mData);

    const int MEMBER_COUNT = 2;
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_EXPOSURE_TIME_RANGE);
    if (entry.count == 0 || entry.count != MEMBER_COUNT) {
        return NAME_NOT_FOUND;
    }

    exposureTimeRange.min = entry.data.i32[0];
    exposureTimeRange.max = entry.data.i32[1];
    return OK;
}

int Parameters::setSensitivityGainRange(camera_range_t sensitivityGainRange)
{
    ParameterHelper::AutoWLock wl(mData);
    float values[] = {sensitivityGainRange.min, sensitivityGainRange.max};

    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_SENSITIVITY_GAIN_RANGE, values, ARRAY_SIZE(values));
}

int Parameters::getSensitivityGainRange(camera_range_t& sensitivityGainRange) const
{
    ParameterHelper::AutoRLock rl(mData);

    const int MEMBER_COUNT = 2;
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_SENSITIVITY_GAIN_RANGE);
    if (entry.count == 0 || entry.count != MEMBER_COUNT) {
        return NAME_NOT_FOUND;
    }

    sensitivityGainRange.min = entry.data.f[0];
    sensitivityGainRange.max = entry.data.f[1];
    return OK;
}

int Parameters::setAeConvergeSpeed(camera_converge_speed_t speed)
{
    uint8_t aeSpeed = speed;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_AE_CONVERGE_SPEED, &aeSpeed, 1);
}

int Parameters::getAeConvergeSpeed(camera_converge_speed_t& speed) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_AE_CONVERGE_SPEED);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    speed = (camera_converge_speed_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAwbConvergeSpeed(camera_converge_speed_t speed)
{
    uint8_t awbSpeed = speed;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_CONVERGE_SPEED, &awbSpeed, 1);
}

int Parameters::getAwbConvergeSpeed(camera_converge_speed_t& speed) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_CONVERGE_SPEED);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    speed = (camera_converge_speed_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAeConvergeSpeedMode(camera_converge_speed_mode_t mode)
{
    uint8_t speedMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_AE_CONVERGE_SPEED_MODE, &speedMode, 1);
}

int Parameters::getAeConvergeSpeedMode(camera_converge_speed_mode_t& mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_AE_CONVERGE_SPEED_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    mode = (camera_converge_speed_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAwbConvergeSpeedMode(camera_converge_speed_mode_t mode)
{
    uint8_t speedMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AWB_CONVERGE_SPEED_MODE, &speedMode, 1);
}

int Parameters::getAwbConvergeSpeedMode(camera_converge_speed_mode_t& mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AWB_CONVERGE_SPEED_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    mode = (camera_converge_speed_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setMakernoteData(const void* data, unsigned int size)
{
    Check(!data || size == 0, BAD_VALUE, "%s, invalid parameters", __func__);
    ParameterHelper::AutoWLock wl(mData);

    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_MAKERNOTE_DATA, (uint8_t*)data, size);
}

int Parameters::getMakernoteData(void* data, unsigned int* size) const
{
    Check(!data || !size, BAD_VALUE, "%s, invalid parameters", __func__);
    ParameterHelper::AutoRLock rl(mData);

    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_MAKERNOTE_DATA);
    if (entry.count > 0) {
        MEMCPY_S(data, *size, entry.data.u8, entry.count);
        *size = entry.count;
    } else {
        return NAME_NOT_FOUND;
    }

    return OK;
}

int Parameters::setCustomAicParam(const void* data, unsigned int length)
{
    Check(!data, BAD_VALUE, "%s, invalid parameters", __func__);
    ParameterHelper::AutoWLock wl(mData);

    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_CUSTOM_AIC_PARAM, (uint8_t*)data, length);
}

int Parameters::getCustomAicParam(void* data, unsigned int* length) const
{
    Check(!data || !length, BAD_VALUE, "%s, invalid parameters", __func__);
    ParameterHelper::AutoRLock rl(mData);

    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_CUSTOM_AIC_PARAM);
    if (entry.count > 0) {
        MEMCPY_S(data, *length, entry.data.u8, entry.count);
        *length = entry.count;
    } else {
        return NAME_NOT_FOUND;
    }

    return OK;
}

int Parameters::setMakernoteMode(camera_makernote_mode_t mode)
{
    uint8_t mknMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_MAKERNOTE_MODE, &mknMode, 1);
}

int Parameters::getMakernoteMode(camera_makernote_mode_t &mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_MAKERNOTE_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    mode = (camera_makernote_mode_t)entry.data.u8[0];

    return OK;
}

int Parameters::setIspControl(uint32_t ctrlId, void* data)
{
    uint32_t size = IspControlUtils::getSizeById(ctrlId);
    uint32_t tag = IspControlUtils::getTagById(ctrlId);
    Check(size == 0, BAD_VALUE, "Unsupported ISP control id:%u", ctrlId);

    ParameterHelper::AutoWLock wl(mData);
    if (data == NULL) {
        return ParameterHelper::getMetadata(mData).erase(tag);
    }
    return ParameterHelper::getMetadata(mData).update(tag, (uint8_t*)data, size);
}

int Parameters::getIspControl(uint32_t ctrlId, void* data) const
{
    uint32_t size = IspControlUtils::getSizeById(ctrlId);
    uint32_t tag = IspControlUtils::getTagById(ctrlId);
    Check(size == 0, BAD_VALUE, "Unsupported ISP control id:%u", ctrlId);

    ParameterHelper::AutoRLock rl(mData);

    auto entry = ParameterHelper::getMetadataEntry(mData, tag);
    if (entry.count != size) {
        return NAME_NOT_FOUND;
    }

    if (data != NULL) {
        MEMCPY_S(data, size, entry.data.u8, size);
    }

    return OK;
}

int Parameters::setEnabledIspControls(const set<uint32_t>& ctrlIds)
{
    ParameterHelper::AutoWLock wl(mData);

    size_t size = ctrlIds.size();
    if (size == 0) {
        return ParameterHelper::getMetadata(mData).erase(INTEL_CONTROL_ISP_ENABLED_CTRL_IDS);
    }

    int32_t data[size];
    int index = 0;
    for (auto ctrlId : ctrlIds) {
        data[index] = ctrlId;
        index++;
    }

    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_ISP_ENABLED_CTRL_IDS, data, size);
}

int Parameters::getEnabledIspControls(set<uint32_t>& ctrlIds) const
{
    ctrlIds.clear();

    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_ISP_ENABLED_CTRL_IDS);
    for (size_t i = 0; i < entry.count; i++) {
        ctrlIds.insert((uint32_t)entry.data.i32[i]);
    }

    return OK;
}

int Parameters::setLtmTuningData(const void* data)
{
    const uint32_t size = 264; // FIXME: Will be fixed after turning data header file released.
    const uint32_t tag = INTEL_CONTROL_LTM_TUNING_DATA;

    ParameterHelper::AutoWLock wl(mData);
    if (data == NULL) {
        return ParameterHelper::getMetadata(mData).erase(tag);
    }
    return ParameterHelper::getMetadata(mData).update(tag, (uint8_t*)data, size);
}

int Parameters::getLtmTuningData(void* data) const
{
    const uint32_t size = 264; // FIXME: Will be fixed after turning data header file released.
    const uint32_t tag = INTEL_CONTROL_LTM_TUNING_DATA;

    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, tag);
    if (entry.count != size) {
        return NAME_NOT_FOUND;
    }

    if (data != NULL) {
        MEMCPY_S(data, size, entry.data.u8, size);
    }

    return OK;
}

int Parameters::setDigitalZoomRatio(float ratio)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_DIGITAL_ZOOM_RATIO, &ratio, 1);
}

int Parameters::getDigitalZoomRatio(float& ratio) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_DIGITAL_ZOOM_RATIO);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    ratio = entry.data.f[0];
    return OK;
}

int Parameters::setLdcMode(camera_ldc_mode_t mode)
{
    uint8_t ldcMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_LDC_MODE, &ldcMode, 1);
}

int Parameters::getLdcMode(camera_ldc_mode_t &mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_LDC_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    mode = (camera_ldc_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setRscMode(camera_rsc_mode_t mode)
{
    uint8_t rscMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_RSC_MODE, &rscMode, 1);
}

int Parameters::getRscMode(camera_rsc_mode_t &mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_RSC_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    mode = (camera_rsc_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setFlipMode(camera_flip_mode_t mode)
{
    uint8_t flipMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_FLIP_MODE, &flipMode, 1);
}

int Parameters::getFlipMode(camera_flip_mode_t &mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_FLIP_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    mode = (camera_flip_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setRun3ACadence(int cadence)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_RUN3_A_CADENCE, &cadence, 1);
}

int Parameters::getRun3ACadence(int &cadence) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_RUN3_A_CADENCE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    cadence = entry.data.i32[0];
    return OK;
}

int Parameters::setFisheyeDewarpingMode(camera_fisheye_dewarping_mode_t mode)
{
    uint8_t dewarpingMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_FISHEYE_DEWARPING_MODE, &dewarpingMode, 1);
}

int Parameters::getFisheyeDewarpingMode(camera_fisheye_dewarping_mode_t &mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_FISHEYE_DEWARPING_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    mode = (camera_fisheye_dewarping_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAeDistributionPriority(camera_ae_distribution_priority_t priority)
{
    uint8_t distributionPriority = priority;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_AE_DISTRIBUTION_PRIORITY, &distributionPriority, 1);
}

int Parameters::getAeDistributionPriority(camera_ae_distribution_priority_t& priority) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_AE_DISTRIBUTION_PRIORITY);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    priority = (camera_ae_distribution_priority_t)entry.data.u8[0];
    return OK;
}

int Parameters::setYuvColorRangeMode(camera_yuv_color_range_mode_t colorRange)
{
    uint8_t mode = colorRange;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_YUV_COLOR_RANGE, &mode, 1);
}

int Parameters::getYuvColorRangeMode(camera_yuv_color_range_mode_t& colorRange) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_YUV_COLOR_RANGE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    colorRange = (camera_yuv_color_range_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setJpegQuality(uint8_t quality)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_QUALITY, &quality, 1);
}

int Parameters::getJpegQuality(uint8_t *quality) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_QUALITY);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    *quality = entry.data.u8[0];
    return OK;
}

int Parameters::setJpegThumbnailQuality(uint8_t quality)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_THUMBNAIL_QUALITY, &quality, 1);
}

int Parameters::getJpegThumbnailQuality(uint8_t *quality) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_THUMBNAIL_QUALITY);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    *quality = entry.data.u8[0];
    return OK;
}

int Parameters::setJpegThumbnailSize(const camera_resolution_t& res)
{
    int size[2] = {res.width, res.height};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_THUMBNAIL_SIZE, size, 2);
}

int Parameters::getJpegThumbnailSize(camera_resolution_t& res) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_THUMBNAIL_SIZE);
    if (entry.count != 2) {
        return NAME_NOT_FOUND;
    }
    res.width  = entry.data.i32[0];
    res.height = entry.data.i32[1];
    return OK;
}

int Parameters::setJpegRotation(int rotation)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_ORIENTATION, &rotation, 1);
}

int Parameters::getJpegRotation(int &rotation) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_ORIENTATION);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    rotation = entry.data.i32[0];
    return OK;
}

int Parameters::setJpegGpsCoordinates(const double *coordinates)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_GPS_COORDINATES, coordinates, 3);
}

int Parameters::getJpegGpsLatitude(double &latitude) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_GPS_COORDINATES);
    if (entry.count != 3) {
        return NAME_NOT_FOUND;
    }
    latitude = entry.data.d[0];
    return OK;
}

int Parameters::getJpegGpsLongitude(double &longitude) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_GPS_COORDINATES);
    if (entry.count != 3) {
        return NAME_NOT_FOUND;
    }
    longitude = entry.data.d[1];
    return OK;
}

int Parameters::getJpegGpsAltitude(double &altitude) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_GPS_COORDINATES);
    if (entry.count != 3) {
        return NAME_NOT_FOUND;
    }
    altitude = entry.data.d[2];
    return OK;
}

int Parameters::setJpegGpsTimeStamp(int64_t  timestamp)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_GPS_TIMESTAMP, &timestamp, 1);
}

int Parameters::getJpegGpsTimeStamp(int64_t &timestamp) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_GPS_TIMESTAMP);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    timestamp = entry.data.i32[0];
    return OK;
}

int Parameters::setJpegGpsProcessingMethod(int processMethod)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_GPS_PROCESSING_METHOD, &processMethod, 1);
}

int Parameters::getJpegGpsProcessingMethod(int &processMethod) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_GPS_PROCESSING_METHOD);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    processMethod = entry.data.u8[0];
    return OK;
}

int Parameters::setJpegGpsProcessingMethod(const char* processMethod)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_JPEG_GPS_PROCESSING_METHOD, (const uint8_t*)processMethod, strlen(processMethod) + 1);
}

int Parameters::getJpegGpsProcessingMethod(int size, char* processMethod) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_JPEG_GPS_PROCESSING_METHOD);
    if (entry.count <= 0) {
        return NAME_NOT_FOUND;
    }
    MEMCPY_S(processMethod, size, entry.data.u8, entry.count);
    return OK;
}

int Parameters::setImageEffect(camera_effect_mode_t  effect)
{
    uint8_t effectmode = effect;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_CONTROL_EFFECT_MODE, &effectmode, 1);
}

int Parameters::getImageEffect(camera_effect_mode_t &effect) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_CONTROL_EFFECT_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    effect = (camera_effect_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setVideoStabilizationMode(camera_video_stabilization_mode_t mode)
{
    uint8_t dvsMode = mode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_CONTROL_VIDEO_STABILIZATION_MODE, &dvsMode, 1);
}

int Parameters::getVideoStabilizationMode(camera_video_stabilization_mode_t &mode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_CONTROL_VIDEO_STABILIZATION_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    mode = (camera_video_stabilization_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::getFocalLength(float &focal) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_LENS_FOCAL_LENGTH);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    focal = (float)entry.data.f[0];
    return OK;
}

int Parameters::setFocalLength(float focal)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_LENS_FOCAL_LENGTH, &focal, 1);
}

int Parameters::getAperture(float &aperture) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_LENS_APERTURE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    aperture = entry.data.f[0];
    return OK;
}

int Parameters::setAperture(float aperture)
{
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_LENS_APERTURE, &aperture, 1);
}

int Parameters::setAfMode(camera_af_mode_t afMode)
{
    uint8_t mode = afMode;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AF_MODE, &mode, 1);
}

int Parameters::getAfMode(camera_af_mode_t& afMode) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AF_MODE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    afMode = (camera_af_mode_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAfTrigger(camera_af_trigger_t afTrigger)
{
    uint8_t trigger = afTrigger;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AF_TRIGGER, &trigger, 1);
}

int Parameters::getAfTrigger(camera_af_trigger_t& afTrigger) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AF_TRIGGER);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    afTrigger = (camera_af_trigger_t)entry.data.u8[0];
    return OK;
}

int Parameters::setAfRegions(camera_window_list_t afRegions)
{
    ParameterHelper::AutoWLock wl(mData);
    return setRegions(ParameterHelper::getMetadata(mData), afRegions, CAMERA_AF_REGIONS);
}

int Parameters::getAfRegions(camera_window_list_t& afRegions) const
{
    ParameterHelper::AutoRLock rl(mData);
    return getRegions(ParameterHelper::getMetadataEntry(mData, CAMERA_AF_REGIONS), afRegions);
}

int Parameters::setAfState(camera_af_state_t afState)
{
    uint8_t state = afState;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_AF_STATE, &state, 1);
}

int Parameters::getAfState(camera_af_state_t& afState) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_AF_STATE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    afState = (camera_af_state_t)entry.data.u8[0];
    return OK;
}

int Parameters::setLensState(bool lensMoving)
{
    uint8_t state = (lensMoving) ? 1 : 0;
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_LENS_STATE, &state, 1);
}

int Parameters::getLensState(bool& lensMoving) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_LENS_STATE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }
    lensMoving = (entry.data.u8[0] > 0);
    return OK;
}

int Parameters::getWFOV(uint8_t& wfov) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_INFO_WFOV);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    wfov = entry.data.u8[0];
    return OK;
}

int Parameters::getSensorMountType(camera_mount_type_t& sensorMountType) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_INFO_SENSOR_MOUNT_TYPE);
    if (entry.count != 1) {
        return NAME_NOT_FOUND;
    }

    sensorMountType = (camera_mount_type_t)entry.data.u8[0];
    return OK;
}

int Parameters::setViewProjection(camera_view_projection_t viewProjection)
{
    ParameterHelper::AutoRLock rl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_VIEW_PROJECTION, (uint8_t *)&viewProjection, sizeof(viewProjection));
}

int Parameters::getViewProjection(camera_view_projection_t& viewProjection) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_VIEW_PROJECTION);
    if (entry.count <= 0) {
        return NAME_NOT_FOUND;
    }
    MEMCPY_S(&viewProjection, sizeof(camera_view_projection_t), entry.data.u8, entry.count);
    return OK;
}

int Parameters::setViewFineAdjustments(camera_view_fine_adjustments_t viewFineAdj)
{
    ParameterHelper::AutoRLock rl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_VIEW_FINE_ADJUSTMENTS, (uint8_t *)&viewFineAdj, sizeof(viewFineAdj));
}

int Parameters::getViewFineAdjustments(camera_view_fine_adjustments_t& viewFineAdj) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_VIEW_FINE_ADJUSTMENTS);
    if (entry.count <= 0) {
        return NAME_NOT_FOUND;
    }
    MEMCPY_S(&viewFineAdj, sizeof(camera_view_fine_adjustments_t), entry.data.u8, entry.count);
    return OK;
}

int Parameters::setViewRotation(camera_view_rotation_t viewRotation)
{
    ParameterHelper::AutoRLock rl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_VIEW_ROTATION, (uint8_t *)&viewRotation, sizeof(viewRotation));
}

int Parameters::getViewRotation(camera_view_rotation_t& viewRotation) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_VIEW_ROTATION);
    if (entry.count <= 0) {
        return NAME_NOT_FOUND;
    }
    MEMCPY_S(&viewRotation, sizeof(camera_view_rotation_t), entry.data.u8, entry.count);
    return OK;
}

int Parameters::setCameraRotation(camera_view_rotation_t cameraRotation)
{
    ParameterHelper::AutoRLock rl(mData);
    return ParameterHelper::getMetadata(mData).update(INTEL_CONTROL_CAMERA_ROTATION, (uint8_t *)&cameraRotation, sizeof(cameraRotation));
}

int Parameters::getCameraRotation(camera_view_rotation_t& cameraRotation) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, INTEL_CONTROL_CAMERA_ROTATION);
    if (entry.count <= 0) {
        return NAME_NOT_FOUND;
    }
    MEMCPY_S(&cameraRotation, sizeof(camera_view_rotation_t), entry.data.u8, entry.count);
    return OK;
}

// User can set envrionment and then call api to update the debug level.
int Parameters::updateDebugLevel()
{
    Log::setDebugLevel();
    return OK;
}

int Parameters::setCropRegion(camera_crop_region_t cropRegion)
{
    int values[] = {cropRegion.flag, cropRegion.x, cropRegion.y};
    ParameterHelper::AutoWLock wl(mData);
    return ParameterHelper::getMetadata(mData).update(CAMERA_SCALER_CROP_REGION, values, ARRAY_SIZE(values));
}

int Parameters::getCropRegion(camera_crop_region_t& cropRegion) const
{
    ParameterHelper::AutoRLock rl(mData);
    auto entry = ParameterHelper::getMetadataEntry(mData, CAMERA_SCALER_CROP_REGION);
    if (entry.count <= 0) {
        return NAME_NOT_FOUND;
    }
    cropRegion.flag = entry.data.i32[0];
    cropRegion.x = entry.data.i32[1];
    cropRegion.y = entry.data.i32[2];
    return OK;
}

} // end of namespace icamera
