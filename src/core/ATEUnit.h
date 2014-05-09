/*
 * Copyright (C) 2018 Intel Corporation.
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

#pragma once

#include <ia_types.h>
extern "C" {
#include <ia_cipf/ia_cipf_iterator.h>
#include <ia_cipf/ia_cipf_pipe.h>
}

namespace icamera {

/**
 * This class is for ATE feature including:
 * 1. compress frame, PAL data and kernel list together
 * 2. bypass p2p for RGBS statistics
 */

class ATEUnit {
public:
    static const int ISP_KERNEL_MAX_COUNT = 128;
    static const int ATE_HEADER_SIZE = 128;
    static const int ATE_ISP_PARAM_DATA_MAX_SIZE = 8 * 1024 * 1024;
    static int compressATEBuf(ia_binary_data pal, const std::vector<uint32_t> &kernelVec, void *buf);
    static int getATEPayloadSize() { return sizeof(uint32_t) * ISP_KERNEL_MAX_COUNT + ATE_HEADER_SIZE +
                                     ATE_ISP_PARAM_DATA_MAX_SIZE; }
    static int getPublicStats(ia_cipf_pipe *pipe, ia_cipf_iterator_t *iterator,
                                ia_uid stageId, ia_binary_data *statistics);

private:
    ATEUnit();

    DISALLOW_COPY_AND_ASSIGN(ATEUnit);

};
} // namespace icamera
