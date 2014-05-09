/*
 * Copyright (C) 2016-2017 Intel Corporation
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

#ifndef HAL_LINUX_ADAPT_UNITTESTS_SRC_TEST_PARAMETERIZATION_H_
#define HAL_LINUX_ADAPT_UNITTESTS_SRC_TEST_PARAMETERIZATION_H_

#include <vector>
#include <string>

#include "camera/camera_metadata.h"

const uint32_t MAX_NUM_CAMERAS = 2;
const uint32_t MAX_NUM_STREAMS = 3;

namespace Parameterization {

struct TestParam {
    TestParam() :
        cameraId(0),
        width(0),
        height(0),
        format(0) {}
    TestParam(int aCameraId, int aWidth, int aHeight, int aFormat) :
        cameraId(aCameraId),
        width(aWidth),
        height(aHeight),
        format(aFormat) {}
    int cameraId;
    int width;
    int height;
    int format;
};

struct MultiCameraTestParam {
    TestParam params[MAX_NUM_CAMERAS];
};

struct MultiStreamsTestParam {
    TestParam params[MAX_NUM_STREAMS];
};

struct MetadataTestParam {
    MetadataTestParam() :
        tag(0),
        value(0) {}
    uint32_t tag;
    uint8_t value;
};

struct ImageSizeDescSort {
    bool operator() (const TestParam &lh, const TestParam &rh)
    {
        // Sort larger image size first
        return (lh.width * lh.height > rh.width * rh.height);
    }
};

// Operator for printing TestParam struct fields in GTest
::std::ostream& operator<<(::std::ostream& os, const TestParam& testParam);

::std::ostream& operator<<(::std::ostream& os, const MetadataTestParam& testParam);

::std::ostream& operator<<(::std::ostream& os, const MultiStreamsTestParam& testParam);

std::vector<TestParam> getCameraValues();
std::vector<TestParam> getResolutionValues(int format, bool largestOnly = false);
std::vector<MultiCameraTestParam> getDualResolutionValues(int format);

std::vector<MultiStreamsTestParam> getMultiResolutionValues(int _1stformat, int _2ndformat);

std::vector<MetadataTestParam> getMetadataTestEntries();

// Function pointer definition for the factory functions
typedef std::vector<TestParam> (*SupportedStreamsFactoryFunc)(int);
// Wrapper function for calling the factories with pararmeters
std::vector<TestParam> getSupportedStreams(SupportedStreamsFactoryFunc factory, int camId);

} // namespace Parameterization

#endif /* HAL_LINUX_ADAPT_UNITTESTS_SRC_TEST_PARAMETERIZATION_H_ */
