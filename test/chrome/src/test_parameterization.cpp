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

/*
 * this file has a set of functions which fill and return params for tests
 */

#include <math.h>
#include "test_parameterization.h"
#include "test_stream_factory.h"
#include "test_utils.h"
#include <iostream>
#include <iomanip> // setw()
#include <hardware/camera3.h>
#include <algorithm>

using namespace android;
namespace TSF = TestStreamFactory;

extern camera_module_t *HAL_MODULE_INFO_SYM_PTR;
extern const char *gTestArgv[];
extern int gTestArgc;

namespace Parameterization {

std::vector<TestParam> getCameraValues(void)
{
    int numCameras = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();
    std::vector<TestParam> cameras;
    for (int i = 0; i < numCameras; i++) {
        TestParam param;
        param.cameraId = i;
        cameras.push_back(param);
    }
    return cameras;
}

/**
 * \brief Overloading getSupportedStreams()
 *
 * For getting supported streams list via a Factory function provided by the
 * test case.
 *
 * \param factory Non-null function pointer to a Factory function
 * \return A vector of TestParam objects descrbing the camera (sensor) properties
 */
std::vector<TestParam> getSupportedStreams(SupportedStreamsFactoryFunc factory, int camId)
{
    EXPECT_NE(factory, nullptr);

    std::vector<TestParam> streams;

    streams = factory(camId);
    return streams;
}

std::vector<TestParam> getResolutionValues(int format, bool largestOnly)
{
    int numCameras = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();
    std::vector<TestParam> params;
    std::vector<TestParam> largestResolutions;

    for (int i = 0; i < numCameras; i++) {
        TestParam param;
        param.cameraId = i;

        // get the static metadata, which has available stream configs
        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(i, &ac2info);
        const camera_metadata_t *meta = ac2info.static_camera_characteristics;

        if (meta == nullptr) {
            PRINTLN("Test startup issue - no metadata available!");
            return params;
        }

        camera_metadata_ro_entry_t entry;
        entry.count = 0;
        int ret = find_camera_metadata_ro_entry(meta,
                                 ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                                 &entry);

        if (ret != OK) {
            PRINTLN("Test startup issue - no stream configurations");
            return params;
        }

        int count = entry.count;
        const int32_t *availStreamConfig = entry.data.i32;
        if (count < 4 || availStreamConfig == nullptr) {
            PRINTLN("Test startup issue - not enough valid stream configurations");
        }

        // find configs to test
        for (uint32_t j = 0; j < (uint32_t)count; j += 4) {
            if (availStreamConfig[j] == format &&
                availStreamConfig[j+3] == CAMERA3_STREAM_OUTPUT) {
                param.width = availStreamConfig[j + 1];
                param.height = availStreamConfig[j + 2];
                params.push_back(param);
            }
        }

        if (largestOnly) {
            // sort
            Parmz::ImageSizeDescSort compareImageSize;
            std::sort(params.begin(), params.end(), compareImageSize);
            TestParam param = params[0]; // pick largest
            params.clear();              // clear the vector
            largestResolutions.push_back(param); // add just largest
        }
    }

    if (largestOnly)
        return largestResolutions;
    else
        return params;
}

void pushgMetadataTestEntries (std::vector<MetadataTestParam> &entries, uint32_t tag, uint8_t value_start, uint8_t value_end)
{
    MetadataTestParam entry;
    for (uint8_t mode = value_start; mode <= value_end; mode++) {
        entry.tag = tag;
        entry.value = mode;
        entries.push_back(entry);
    }
}

std::vector<MetadataTestParam> getMetadataTestEntries()
{
    std::vector<MetadataTestParam> entries;

    // FIXME only test controls that are listed as supported in static metadata
    pushgMetadataTestEntries(entries, ANDROID_CONTROL_AF_MODE, ANDROID_CONTROL_AF_MODE_OFF, ANDROID_CONTROL_AF_MODE_EDOF);
    pushgMetadataTestEntries(entries, ANDROID_CONTROL_AWB_MODE, ANDROID_CONTROL_AWB_MODE_OFF, ANDROID_CONTROL_AWB_MODE_SHADE);
    pushgMetadataTestEntries(entries, ANDROID_CONTROL_AE_MODE, ANDROID_CONTROL_AE_MODE_OFF, ANDROID_CONTROL_AE_MODE_ON_AUTO_FLASH_REDEYE);
    pushgMetadataTestEntries(entries, ANDROID_CONTROL_EFFECT_MODE, ANDROID_CONTROL_EFFECT_MODE_OFF, ANDROID_CONTROL_EFFECT_MODE_AQUA);
    pushgMetadataTestEntries(entries, ANDROID_CONTROL_SCENE_MODE, ANDROID_CONTROL_SCENE_MODE_DISABLED, ANDROID_CONTROL_SCENE_MODE_FACE_PRIORITY_LOW_LIGHT);

    return entries;
}

bool isEqual(float a, float b, float relError)
{
    if (fabs(a) < fabs(b)) {
        return (fabs((a-b)/b) < relError) ? true : false;
    } else {
        return (fabs((a-b)/a) < relError) ? true : false;
    }
}

std::vector<MultiStreamsTestParam> getMultiResolutionValues(int _1stformat, int _2ndformat)
{
    int numCameras = HAL_MODULE_INFO_SYM_PTR->get_number_of_cameras();
    std::vector<MultiStreamsTestParam> params;
    const int32_t SCALER_ENTITY_SIZE = 4;
    for (int i = 0; i < numCameras; i++) {
        // get the static metadata, which has available stream configs
        struct camera_info ac2info;
        HAL_MODULE_INFO_SYM_PTR->get_camera_info(i, &ac2info);
        const camera_metadata_t *meta = ac2info.static_camera_characteristics;
        if (meta == nullptr) {
            PRINTLN("Test startup issue - no metadata available!");
            return params;
        }

        camera_metadata_ro_entry_t entry;
        entry.count = 0;
        int ret = find_camera_metadata_ro_entry(meta,
                             ANDROID_SCALER_AVAILABLE_STREAM_CONFIGURATIONS,
                             &entry);
        if (ret != OK) {
            PRINTLN("Test startup issue - no stream configurations");
            return params;
        }

        int count = entry.count;
        const int32_t *availStreamConfig = entry.data.i32;
        if (count < SCALER_ENTITY_SIZE || availStreamConfig == nullptr) {
            PRINTLN("Test startup issue - not enough valid stream configurations");
        }

        MultiStreamsTestParam dualpara;
        dualpara.params[0].cameraId = i;
        dualpara.params[0].format = _1stformat;
        dualpara.params[1].cameraId = i;
        dualpara.params[1].format = _2ndformat;
        float ratioA, ratioB;
        for (uint32_t i = 0; i < (uint32_t)count; i += SCALER_ENTITY_SIZE) {
            if (availStreamConfig[i] == _1stformat &&
                availStreamConfig[i+3] == CAMERA3_STREAM_OUTPUT) {
                dualpara.params[0].width = availStreamConfig[i + 1];
                dualpara.params[0].height = availStreamConfig[i + 2];
                if (dualpara.params[0].width == 0 || dualpara.params[0].height == 0) {
                    PRINTLN("Test startup issue -  wrong resolution");
                    break;
                }
                ratioA = (float)dualpara.params[0].width/(float)dualpara.params[0].height;
                for (uint32_t j = 0; j < (uint32_t)count; j += SCALER_ENTITY_SIZE) {
                    if (availStreamConfig[j] == _2ndformat &&
                        availStreamConfig[j+3] == CAMERA3_STREAM_OUTPUT) {
                        dualpara.params[1].width = availStreamConfig[j + 1];
                        dualpara.params[1].height = availStreamConfig[j + 2];
                        if (dualpara.params[0].width == 0 || dualpara.params[0].height == 0) {
                            PRINTLN("Test startup issue -  wrong resolution");
                            break;
                        }
                        ratioB = (float)dualpara.params[1].width/(float)dualpara.params[1].height;
                        if (isEqual(ratioA, ratioB, 0.00001)) {
                            params.push_back(dualpara);
                        }
                    }
                }
            }
        }
    }
    return params;
}

// Operator to allow GTest to print human-readable TestParam values
// for example in --gtest_list_test
::std::ostream& operator<<(::std::ostream& os, const TestParam& testParam) {
    // Longest string "5248x3936", hence:
    const int MAX_STRING_LEN_WIDTH = 4;
    const int MAX_STRING_LEN_HEIGHT = 4;

    // Get the format as string representation using an Android util function
    const int MAX_METADATA_VALUE_LEN = 256;
    char metadataValue[MAX_METADATA_VALUE_LEN];
    int ret = camera_metadata_enum_snprint(ANDROID_SCALER_AVAILABLE_FORMATS,
                                           testParam.format,
                                           metadataValue,
                                           MAX_METADATA_VALUE_LEN);

    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue, "N/A\0", 4);
    }

    std::ios_base::fmtflags origFlags = os.flags();

    // Attempt pretty formatting
    os << std::setw(MAX_STRING_LEN_WIDTH) << testParam.width << "x"
        << std::left << std::setw(MAX_STRING_LEN_HEIGHT + 1) << testParam.height
        << " camera ID: " << testParam.cameraId
        << " format: " << metadataValue;

    // Restore the format flags
    os.setf(origFlags);
    return os;
}

::std::ostream& operator<<(::std::ostream& os, const MetadataTestParam& testParam) {
    // Longest string "5248x3936", hence:
    const int MAX_STRING_LEN_WIDTH = 4;
    const int MAX_STRING_LEN_HEIGHT = 4;
    const int MAX_METADATA_VALUE_LEN = 256;
    char metadataValue[MAX_METADATA_VALUE_LEN];

    std::ios_base::fmtflags origFlags = os.flags();

    int ret = camera_metadata_enum_snprint(testParam.tag,
                                           testParam.value,
                                           metadataValue,
                                           MAX_METADATA_VALUE_LEN);

    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue, "N/A\0", 4);
    }

    // Attempt pretty formatting
    os << std::setw(MAX_STRING_LEN_WIDTH)
       << "tag: " << get_camera_metadata_tag_name(testParam.tag)
       << " value: " << std::left << std::setw(MAX_STRING_LEN_HEIGHT + 1) << metadataValue;

    // Restore the format flags
    os.setf(origFlags);
    return os;
}

::std::ostream& operator<<(::std::ostream& os, const MultiCameraTestParam& testParam) {
    // Longest string "5248x3936", hence:
    const int MAX_STRING_LEN_WIDTH = 4;
    const int MAX_STRING_LEN_HEIGHT = 4;

    // Get the format as string representation using an Android util function
    const int MAX_METADATA_VALUE_LEN = 256;
    char metadataValue0[MAX_METADATA_VALUE_LEN];
    char metadataValue1[MAX_METADATA_VALUE_LEN];
    int ret = camera_metadata_enum_snprint(ANDROID_SCALER_AVAILABLE_FORMATS,
                                           testParam.params[0].format,
                                           metadataValue0,
                                           MAX_METADATA_VALUE_LEN);

    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue0, "N/A\0", 4);
    }

    ret = camera_metadata_enum_snprint(ANDROID_SCALER_AVAILABLE_FORMATS,
                                           testParam.params[1].format,
                                           metadataValue1,
                                           MAX_METADATA_VALUE_LEN);

    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue1, "N/A\0", 4);
    }

    std::ios_base::fmtflags origFlags = os.flags();

    // Attempt pretty formatting
    os  << " camera ID: " << testParam.params[0].cameraId
        << std::setw(MAX_STRING_LEN_WIDTH) << testParam.params[0].width << "x"
        << std::left << std::setw(MAX_STRING_LEN_HEIGHT + 1) << testParam.params[0].height
        << " format: " << metadataValue0
        << " camera ID: " << testParam.params[0].cameraId
           << std::setw(MAX_STRING_LEN_WIDTH) << testParam.params[1].width << "x"
        << std::left << std::setw(MAX_STRING_LEN_HEIGHT + 1) << testParam.params[1].height
        << " format: " << metadataValue1;

    // Restore the format flags
    os.setf(origFlags);
    return os;
}

::std::ostream& operator<<(::std::ostream& os, const MultiStreamsTestParam& testParam) {
    // Get the format as string representation using an Android util function
    const int MAX_METADATA_VALUE_LEN = 256;
    char metadataValue0[MAX_METADATA_VALUE_LEN];
    char metadataValue1[MAX_METADATA_VALUE_LEN];
    char metadataValue2[MAX_METADATA_VALUE_LEN];
    int ret = camera_metadata_enum_snprint(ANDROID_SCALER_AVAILABLE_FORMATS,
                                           testParam.params[0].format,
                                           metadataValue0,
                                           MAX_METADATA_VALUE_LEN);

    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue0, "N/A\0", 4);
    }

    ret = camera_metadata_enum_snprint(ANDROID_SCALER_AVAILABLE_FORMATS,
                                           testParam.params[1].format,
                                           metadataValue1,
                                           MAX_METADATA_VALUE_LEN);
    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue1, "N/A\0", 4);
    }

    ret = camera_metadata_enum_snprint(ANDROID_SCALER_AVAILABLE_FORMATS,
                                           testParam.params[2].format,
                                           metadataValue2,
                                           MAX_METADATA_VALUE_LEN);
    if (ret < 0) {
        // In some cases .format field not set in our tests. Let's not
        // print the error messge from the util function.
        strncpy(metadataValue2, "N/A\0", 4);
    }

    std::ios_base::fmtflags origFlags = os.flags();
    os  << " camera ID: " << testParam.params[0].cameraId << " muti streams."
        <<" 1st size&format("
        << testParam.params[0].width << "x"
        << testParam.params[0].height
        << "," << metadataValue0
        << ") 2nd size&format("
        << testParam.params[1].width << "x"
        << testParam.params[1].height
        << "," << metadataValue1 << ")";
    if (testParam.params[2].width > 0) {
        os  << " 3nd size&format("
            << testParam.params[2].width << "x"
            << testParam.params[2].height
            << "," << metadataValue2 << ")";
    }
    // Restore the format flags
    os.setf(origFlags);
    return os;
}

} // namespace Parameterization
