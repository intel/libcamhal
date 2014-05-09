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

#include "test_stream_factory.h"
#include <hardware/camera3.h>

namespace Parmz = Parameterization;

namespace TestStreamFactory {

std::vector<Parmz::TestParam> getSupportedStreams(int camId)
{
    std::vector<Parmz::TestParam> streams;

    streams.push_back(Parmz::TestParam(camId, 320,  240,  HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 640,  480,  HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 1280, 720,  HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 1280, 960,  HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 1920, 1080, HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 1600, 1200, HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 2560, 1920, HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 3264, 2448, HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 4096, 3072, HAL_PIXEL_FORMAT_BLOB));
    streams.push_back(Parmz::TestParam(camId, 320,  240,  HAL_PIXEL_FORMAT_YCbCr_420_888));
    streams.push_back(Parmz::TestParam(camId, 640,  480,  HAL_PIXEL_FORMAT_YCbCr_420_888));
    streams.push_back(Parmz::TestParam(camId, 1280, 720,  HAL_PIXEL_FORMAT_YCbCr_420_888));
    streams.push_back(Parmz::TestParam(camId, 1280, 960,  HAL_PIXEL_FORMAT_YCbCr_420_888));
    streams.push_back(Parmz::TestParam(camId, 1600, 1200, HAL_PIXEL_FORMAT_YCbCr_420_888));
    streams.push_back(Parmz::TestParam(camId, 1920, 1080, HAL_PIXEL_FORMAT_YCbCr_420_888));
    streams.push_back(Parmz::TestParam(camId, 320,  240,  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));
    streams.push_back(Parmz::TestParam(camId, 640,  480,  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));
    streams.push_back(Parmz::TestParam(camId, 1280, 720,  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));
    streams.push_back(Parmz::TestParam(camId, 1280, 960,  HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));
    streams.push_back(Parmz::TestParam(camId, 1600, 1200, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));
    streams.push_back(Parmz::TestParam(camId, 1920, 1080, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED));
    return streams;
}

static std::vector<Parmz::MultiStreamsTestParam>
testMultiStresms(int camId, std::vector<struct testPair>& pairs)
{
    std::vector<Parmz::MultiStreamsTestParam> params;
    for (struct testPair & p: pairs) {
        Parmz::MultiStreamsTestParam dualpara;
        dualpara.params[0] =
            Parmz::TestParam(camId, p.res1.width, p.res1.height, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        dualpara.params[1] =
            Parmz::TestParam(camId, p.res2.width, p.res2.height, HAL_PIXEL_FORMAT_YCbCr_420_888);
        params.push_back(dualpara);
    }

    return params;
}

std::vector<Parmz::MultiStreamsTestParam> getMultiStreamsTestParams(int camId)
{
    std::vector<struct testPair> testPairs = {
        {{640, 480}, {640, 480}},
        {{1280, 720}, {1280, 720}},
        {{1280, 720}, {1920, 1080}},
        {{1920, 1080}, {1280, 720}},
        {{1920, 1080}, {1920, 1080}},
    };

    return testMultiStresms(camId, testPairs);
}

std::vector<Parmz::MultiStreamsTestParam> getCameraStreamsTestParams(int camId)
{
    std::vector<struct testPair> testPairs = {
        {{1920, 1080}, {640, 480}},
        {{640, 480}, {1920, 1080}},
    };

    return testMultiStresms(camId, testPairs);
}

static std::vector<Parmz::MultiStreamsTestParam>
testTripleStresms(int camId, std::vector<struct testGroup>& groups)
{
    std::vector<Parmz::MultiStreamsTestParam> params;
    for (struct testGroup & p: groups) {
        Parmz::MultiStreamsTestParam triplepara;
        triplepara.params[0] =
            Parmz::TestParam(camId, p.res1.width, p.res1.height, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        triplepara.params[1] =
            Parmz::TestParam(camId, p.res2.width, p.res2.height, HAL_PIXEL_FORMAT_YCbCr_420_888);
        triplepara.params[2] =
            Parmz::TestParam(camId, p.res3.width, p.res3.height, HAL_PIXEL_FORMAT_BLOB);
        params.push_back(triplepara);
    }

    return params;
}

std::vector<Parmz::MultiStreamsTestParam> getTripleStreamsTestParams(int camId)
{
    std::vector<struct testGroup> testGroups = {
        //preview,    video,       jpeg
        {{320,  240}, {320,  240}, {320, 240}},
        {{640,  480}, {640,  480}, {640, 480}},
        {{1280, 720}, {1280, 720}, {640, 480}},
        {{1280, 720}, {1280, 720}, {1280, 720}},
        {{1280, 720}, {1280, 720}, {1600, 1200}},
        {{1280, 720}, {1280, 720}, {2560, 1920}},
        {{1920, 1080}, {1920, 1080}, {320,  240}},   // 16:9 + 4:3
        {{1920, 1080}, {1920, 1080}, {640,  480}},   // 16:9 + 4:3
        {{1920, 1080}, {1920, 1080}, {1280, 720}},
        {{1920, 1080}, {1920, 1080}, {1280, 960}},   // 16:9 + 4:3
        {{1920, 1080}, {1920, 1080}, {1920, 1080}},
        {{1920, 1080}, {1920, 1080}, {1600, 1200}},  // 16:9 + 4:3
        {{1920, 1080}, {1920, 1080}, {2560, 1920}},  // 16:9 + 4:3
        {{1920, 1080}, {1920, 1080}, {3264, 2448}},  // 16:9 + 4:3
        {{1920, 1080}, {1920, 1080}, {4096, 3072}},
    };

    return testTripleStresms(camId, testGroups);
}

static std::vector<Parmz::MultiStreamsTestParam>
testJpegWithPreview(int camId, std::vector<struct testPair>& pairs)
{
    std::vector<Parmz::MultiStreamsTestParam> jpegParams;
    for (struct testPair & p: pairs) {
        Parmz::MultiStreamsTestParam dualpara;
        dualpara.params[0] =
            Parmz::TestParam(camId, p.res1.width, p.res1.height, HAL_PIXEL_FORMAT_BLOB);
        dualpara.params[1] =
            Parmz::TestParam(camId, p.res2.width, p.res2.height, HAL_PIXEL_FORMAT_IMPLEMENTATION_DEFINED);
        jpegParams.push_back(dualpara);
    }

    return jpegParams;
}

std::vector<Parmz::MultiStreamsTestParam> getJpegTestParams(int camId)
{
    std::vector<struct testPair> testPairs = {
        {{640, 480}, {640, 480}},
        {{1280, 720}, {1280, 720}},
        {{1280, 720}, {1920, 1080}},
        {{1920, 1080}, {1280, 720}},
        {{1920, 1080}, {1920, 1080}},
        {{2560, 1920}, {640, 480}},
        {{2560, 1920}, {320, 240}},
        {{3264, 2448}, {1920, 1080}},
        {{4096, 3072}, {640, 480}},
        {{4096, 3072}, {320, 240}},
    };

    return testJpegWithPreview(camId, testPairs);
}

} // namespace TestStreamFactory
