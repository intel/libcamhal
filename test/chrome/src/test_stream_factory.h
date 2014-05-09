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
#ifndef TEST_STREAM_FACTORY_H
#define TEST_STREAM_FACTORY_H

#include <vector>
#include "test_parameterization.h"


namespace Parmz = Parameterization;

namespace TestStreamFactory {

struct Rec {
    int width;
    int height;
};
struct testPair {
    struct Rec res1;
    struct Rec res2;
};

struct testGroup {
    struct Rec res1;
    struct Rec res2;
    struct Rec res3;
};

std::vector<Parmz::TestParam> getSupportedStreams(int camId);
std::vector<Parmz::MultiStreamsTestParam> getMultiStreamsTestParams(int camId);
std::vector<Parmz::MultiStreamsTestParam> getCameraStreamsTestParams(int camId);
std::vector<Parmz::MultiStreamsTestParam> getJpegTestParams(int camId);
std::vector<Parmz::MultiStreamsTestParam> getTripleStreamsTestParams(int camId);

} // namespace TestStreamFactory

#endif // #ifndef TEST_STREAM_FACTORY_H
