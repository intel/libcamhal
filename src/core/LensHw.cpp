/*
 * Copyright (C) 2016-2018 Intel Corporation
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

#define LOG_TAG "LensHw"

#include "LensHw.h"
#include "iutils/CameraLog.h"
#include "V4l2DeviceFactory.h"
#include "PlatformData.h"

namespace icamera {

LensHw::LensHw(int cameraId):
    mCameraId(cameraId),
    mLensSubdev(nullptr),
    mLastLensPosition(0),
    mLensMovementStartTime(0)
{
    LOG1("@%s", __FUNCTION__);
}

LensHw::~LensHw() {
    LOG1("@%s", __FUNCTION__);
}

int LensHw::init()
{
    LOG1("@%s", __FUNCTION__);
    string lensName = PlatformData::getLensName(mCameraId);
    if (lensName.empty()) {
        LOGD("%s No Lens for camera id:%d ", __func__, mCameraId);
        return OK;
    }

    LOG1("%s camera id:%d lens name:%s", __func__, mCameraId, lensName.c_str());
    string subDevName;
    CameraUtils::getSubDeviceName(lensName.c_str(), subDevName);
    if (!subDevName.empty()) {
        mLensSubdev = V4l2DeviceFactory::getSubDev(mCameraId, subDevName);
        mLensName=lensName;
        return OK;
    }

    LOGW("%s Fail to init lens for camera id:%d lens name:%s", __func__, mCameraId, lensName.c_str());
    return OK;
}

/**
 * focus with absolute value
 */
int LensHw::setFocusPosition(int position)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    mLastLensPosition = position;

    struct timespec t = {};
    clock_gettime(CLOCK_MONOTONIC, &t);

    mLensMovementStartTime = ((long)t.tv_sec) * 1000000 + (long)t.tv_nsec / 1000;

    LOG2("@%s: %d, time %lld", __FUNCTION__, position, mLensMovementStartTime);
    return mLensSubdev->setControl(V4L2_CID_FOCUS_ABSOLUTE, position);
}

/**
 * focus with  relative value
 */
int LensHw::setFocusStep(int steps)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->setControl(V4L2_CID_FOCUS_RELATIVE, steps);
}

int LensHw::getFocusPosition(int &position)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->getControl(V4L2_CID_FOCUS_ABSOLUTE, &position);
}

int LensHw::getFocusStatus(int & /*status*/)
{
    LOG2("@%s", __FUNCTION__);
    return OK;
}

int LensHw::startAutoFocus(void)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->setControl(V4L2_CID_AUTO_FOCUS_START, 1);
}

int LensHw::stopAutoFocus(void)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->setControl(V4L2_CID_AUTO_FOCUS_STOP, 0);
}

int LensHw::getAutoFocusStatus(int &status)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->getControl(V4L2_CID_AUTO_FOCUS_STATUS,
                                    reinterpret_cast<int*>(&status));
}

int LensHw::setAutoFocusRange(int value)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->setControl(V4L2_CID_AUTO_FOCUS_RANGE, value);
}

int LensHw::getAutoFocusRange(int &value)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->getControl(V4L2_CID_AUTO_FOCUS_RANGE, &value);
}

const char* LensHw::getLensName(void)
{
    return mLensName.c_str();
}

/**
 * getLatestPosition
 *
 * returns the latest position commanded to the lens actuator and when this
 * was issued.
 * This method does not query the driver.
 *
 * \param: lensPosition[OUT]: lens position last applied
 * \param: time[OUT]: time in micro seconds when the lens move command was sent.
 */
int LensHw::getLatestPosition(int& lensPosition, unsigned long long& time)
{
    lensPosition = mLastLensPosition;
    time = mLensMovementStartTime;
    return OK;
}

/**
 * Set PWM duty, it is used to control aperture.
 */
int LensHw::setPwmDuty(int pwmDuty)
{
    Check(!mLensSubdev, NO_INIT, "%s: No Lens device inited.", __func__);
    LOG2("@%s", __FUNCTION__);
    return mLensSubdev->setControl(V4L2_CID_PWM_DUTY, pwmDuty);
}


}   // namespace icamera
