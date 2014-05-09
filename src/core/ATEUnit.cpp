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

#define LOG_TAG "ATEUnit"

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "iutils/Errors.h"
#include "ATEUnit.h"

extern "C" {
#include <ia_cipf/ia_cipf_property.h>
#include <ia_cipf/ia_cipf_stage.h>
#include <ia_cipf/ia_cipf_terminal.h>
#include <ia_cipf/ia_cipf_buffer.h>
#include <ia_cipr/ia_cipr_memory.h>
#include <ia_cipr/ia_cipr_assert.h>
}

namespace icamera {

int ATEUnit::compressATEBuf(ia_binary_data pal, const std::vector<uint32_t> &kernelVec, void *buf)
{
    Check((buf == nullptr || pal.data == nullptr || kernelVec.size() > ISP_KERNEL_MAX_COUNT ||
           pal.size > ATE_ISP_PARAM_DATA_MAX_SIZE), UNKNOWN_ERROR,
           "Error: failed to fill pal bin and kernel list");
    /**
     * Following the ATE virtual pipe design, HAL will pack the
     * PAL data and enabled kernel list into data in terminal
     * following the below defined data format:
     * 4 bytes PALD header + 4 bytes overall size + PAL standard format (header(uuid + size) +
     * data + next blob header(uuid + size) + data + ...) +
     * 4 bytes KUID header + 4 bytes kernel count + kernel list format (kernel id + kernel id + ...)
     */
    // fill pal data
    MEMCPY_S((unsigned char *)buf, 4, "PALD", 4);
    *(unsigned int *)((unsigned char *)buf + 4) = pal.size;
    unsigned char *palDataPtr = (unsigned char *)buf + 8;
    MEMCPY_S(palDataPtr, ATE_ISP_PARAM_DATA_MAX_SIZE - 8, pal.data, pal.size);
    // fill kernel list
    unsigned char *kernelUuidHeaderPtr = palDataPtr + pal.size;
    MEMCPY_S(kernelUuidHeaderPtr, 4, "KUID", 4);
    *(unsigned int *)(kernelUuidHeaderPtr + 4) = kernelVec.size();
    unsigned int *kernelUuidPtr = (unsigned int *)(kernelUuidHeaderPtr + 8);
    for (size_t i = 0; i < kernelVec.size(); i++) {
        *(kernelUuidPtr + i) = kernelVec[i];
    }
    return OK;
}

int ATEUnit::getPublicStats(ia_cipf_pipe *pipe, ia_cipf_iterator_t *iterator,
                            ia_uid stageId, ia_binary_data *statistics)
{
    css_err_t ret;
    Check((statistics == nullptr || stageId <= 0), UNKNOWN_ERROR, "Error: invalid params");

    ia_uid uid;
    ia_cipf_stage_t *stage = nullptr;
    do {
        uint32_t i = 0;
        uid = ia_cipf_iteration_enumerate_stages(iterator, i++);
        if (uid == stageId) {
            LOG1("found the stats stage:%d", stageId);
            stage = ia_cipf_pipe_get_stage_by_uid(pipe, uid);
            break;
        }
    } while (uid != 0);

    /**
     * According to ATE virtual pipe design, the mock driver will pack the
     * public statistics output data from ATE server into one param out terminal
     * following the below defined data format:
     * 4 bytes PALD header + 4 bytes overall size + RGBS stats
     */
    for (uint32_t i = 0; i < ia_cipf_stage_get_terminal_count(stage); i++) {
        ia_cipf_terminal_t *terminal = ia_cipf_stage_enumerate_terminals(stage, i);
        if (!ia_cipf_terminal_is_enabled(terminal))
            continue;

        ia_cipf_terminal_type_t type;
        ia_cipf_terminal_get_type(terminal, &type);
        if (type != ia_cipf_terminal_type_param_output)
            continue;

        ia_cipf_buffer_t *buffer = ia_cipf_terminal_get_current_buffer(terminal);
        if (buffer == nullptr)
            continue;

        ia_cipf_payload_t payload;
        ret = ia_cipf_buffer_access_payload(buffer, &payload);
        if (ret == css_err_none && payload.data.cpu_ptr != nullptr && payload.size > 0) {
            if (strncmp((char*)payload.data.cpu_ptr, "PALD", 4) == 0) {
                statistics->size = *(unsigned int *)((unsigned char *)payload.data.cpu_ptr + 4);
                statistics->data = (unsigned char *)payload.data.cpu_ptr + 8;
                LOG1("stage %d, statistics size %d", ia_cipf_stage_get_uid(stage), statistics->size);
                return OK;
            }
        }
    }

    LOGE("could not find valid param out terminal with stats header");
    return UNKNOWN_ERROR;
}

}
