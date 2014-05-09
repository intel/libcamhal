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

#define LOG_TAG "Camera3AMetadata"

#include "Utils.h"

#include "HALv3Header.h"
#include "HALv3Utils.h"
#include "Camera3AMetadata.h"

namespace camera3 {

Camera3AMetadata::Camera3AMetadata(int cameraId) : mCameraId(cameraId)
{
    LOG1("@%s", __func__);

    mIntelAFStateMachine = new IntelAFStateMachine(mCameraId);
    mIntelAEStateMachine = new IntelAEStateMachine(mCameraId);
    mIntelAWBStateMachine = new IntelAWBStateMachine(mCameraId);
}

Camera3AMetadata::~Camera3AMetadata()
{
    LOG1("@%s", __func__);

    delete mIntelAFStateMachine;
    delete mIntelAEStateMachine;
    delete mIntelAWBStateMachine;
}

void Camera3AMetadata::process3Astate(const icamera::Parameters &parameter,
                                      android::CameraMetadata &metadata)
{
    // TODO use it when 3a state API is ready.
    UNUSED(parameter);
    LOG2("@%s", __func__);

    // get 3a info from metadata
    uint8_t afTrigger = ANDROID_CONTROL_AF_TRIGGER_IDLE;
    camera_metadata_entry entry = metadata.find(ANDROID_CONTROL_AF_TRIGGER);
    if (entry.count == 1) {
        afTrigger = entry.data.u8[0];
    }

    uint8_t afMode = ANDROID_CONTROL_AF_MODE_AUTO;
    entry = metadata.find(ANDROID_CONTROL_AF_MODE);
    if (entry.count == 1) {
        afMode = entry.data.u8[0];
    }

    mIntelAFStateMachine->processTriggers(afTrigger, afMode);

    // get AF state
    mIntelAFStateMachine->processResult(icamera::AF_STATE_SUCCESS, true, metadata);

    AeControls aeControls = {
        ANDROID_CONTROL_AE_MODE_ON,
        ANDROID_CONTROL_AE_LOCK_OFF,
        ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER_IDLE,
        ANDROID_CONTROL_SCENE_MODE_DISABLED,
        0
    };
    uint8_t controlMode = ANDROID_CONTROL_MODE_AUTO;
    entry = metadata.find(ANDROID_CONTROL_MODE);
    if (entry.count == 1) {
        controlMode = entry.data.u8[0];
    }

    uint8_t sceneMode = ANDROID_CONTROL_SCENE_MODE_DISABLED;
    entry = metadata.find(ANDROID_CONTROL_SCENE_MODE);
    if (entry.count == 1) {
        sceneMode = entry.data.u8[0];
    }

    entry = metadata.find(ANDROID_CONTROL_AE_MODE);
    if (entry.count == 1) {
        aeControls.aeMode = entry.data.u8[0];
    }

    entry = metadata.find(ANDROID_CONTROL_AE_LOCK);
    if (entry.count == 1) {
        aeControls.aeLock = entry.data.u8[0];
    }

    entry = metadata.find(ANDROID_CONTROL_AE_PRECAPTURE_TRIGGER);
    if (entry.count == 1) {
        aeControls.aePreCaptureTrigger = entry.data.u8[0];
    }

    entry = metadata.find(ANDROID_CONTROL_AE_EXPOSURE_COMPENSATION);
    if (entry.count == 1) {
        aeControls.evCompensation = entry.data.i32[0];
    }

    mIntelAEStateMachine->processState(controlMode, sceneMode, aeControls);

    // get AE state
    mIntelAEStateMachine->processResult(true, metadata);

    AwbControls awbControls = {
        ANDROID_CONTROL_AWB_MODE_AUTO,
        ANDROID_CONTROL_AWB_LOCK_OFF,
        0, 0
    };
    entry = metadata.find(ANDROID_CONTROL_AWB_MODE);
    if (entry.count == 1) {
        awbControls.awbMode = entry.data.u8[0];
    }

    entry = metadata.find(ANDROID_CONTROL_AWB_LOCK);
    if (entry.count == 1) {
        awbControls.awbLock = entry.data.u8[0];
    }

    mIntelAWBStateMachine->processState(controlMode, sceneMode, awbControls);

    // get AWB state
    mIntelAWBStateMachine->processResult(true, metadata);
}

} // namespace camera3
