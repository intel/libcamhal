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

#ifndef __CASE_COMMON_H__
#define __CASE_COMMON_H__

#include <vector>
#include <map>

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include "Parameters.h"

using namespace std;
using namespace icamera;

#define EXPECT_NULL(x)     EXPECT_EQ((void*)0, (x))
#define EXPECT_NOT_NULL(x) EXPECT_NE((void*)0, (x))

#define COLOR_THRESH_H 100
#define COLOR_THRESH_L 50
#define DIFF_THRESH 50

#define OVER_THRESH(x) ((x) - ((x) >>3))
#define MAX3(a, b, c) ((a) > (b))?(((a) > (c))?(a):(c)):(((b) > (c))?(b):(c))

#define  COLOR_NONE                 "\033[0m"
#define  FONT_COLOR_RED             "\033[0;31m"
#define  FONT_COLOR_YELLOW          "\033[0;33m"
#define  FONT_COLOR_BLUE            "\033[1;34m"
#define  BACKGROUND_COLOR_RED       "\033[41m"
#define  BG_RED_FONT_YELLOW         "\033[41;33m"

enum COLOR_ERR{
    NORMAL = 0,
    OVER_RED,
    OVER_GREEN,
    OVER_BLUE,
    BLACK_WHITE,
};

// A map for applying setting "Parameters" at frame "int"
typedef std::map<int, Parameters> ParamList;

const char* getCurrentCameraName();

int getCurrentCameraId();

bool isFeatureSupported(camera_features feature);

int check_image(int cameraId, void *data, unsigned int width,
                    unsigned int height, unsigned int length, unsigned int fmt);
int getRandomValue(int min, int max);
int getISysSupportedStreamConfig(stream_array_t& config);
stream_t camera_device_config_stream_normal(int cameraId, stream_t &config, int memType);
void camhal_qbuf_dqbuf_common(int width, int height, int fmt, int alloc_buffer_count, int dq_buffer_count,
                              int field, ParamList* params = NULL);
void camhal_qbuf_dqbuf(int cameraId, int width, int height, int fmt, int alloc_buffer_count, int dq_buffer_count,
                              int field, ParamList* params = NULL, int total_virtual_channel_camera_num = 0);
void dumpImageBuffer(int cameraId, const camera_buffer_t& buffer);
int prepareStreams(int cameraId, stream_t streams[], int count);
int prepareStillStreams(int cameraId, stream_t streams[], int count);

stream_t getStreamByConfig(const supported_stream_config_t& config);

void test_configure_with_input_format(int inputFmt, int outputFmt, int width, int height);
void test_configure_with_input_size(int inWidth, int inHeight, int outWidth, int outHeight);

#endif
