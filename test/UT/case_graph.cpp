/*
 * Copyright (C) s018 Intel Corporation.
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

#define LOG_TAG "CASE_GRAPH"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "iutils/CameraLog.h"
#include "iutils/Utils.h"
#include "ICamera.h"
#include "PlatformData.h"
#include "MockSysCall.h"
#include "Parameters.h"
#include "case_common.h"
#include "IGraphConfigManager.h"
#ifndef BYPASS_MODE
#include "ia_isp_bxt_types.h"
#endif

TEST_F(camHalTest, graph_hal_common_interface)
{
    int ret = camera_hal_init();
    EXPECT_EQ(ret, 0);

    int cameraId = getCurrentCameraId();

    if (!PlatformData::getGraphConfigNodes(cameraId)) {
        ret = camera_hal_deinit();
        EXPECT_EQ(ret, 0);
        return;
    }

    // Construct common interface for graph config manager
    IGraphConfigManager *GCM = IGraphConfigManager::getInstance(cameraId);

    camera_info_t info;
    get_camera_info(cameraId, info);
    EXPECT_NOT_NULL(info.capability);
    supported_stream_config_array_t configs;
    info.capability->getSupportedStreamConfig(configs);
    EXPECT_TRUE(configs.size() > 0);

    for (int i = 0; i < configs.size(); i++) {
        LOGD("Camera id:%d name:%s format:%s, resolution (%dx%d) field:%d.", cameraId, info.name,
                CameraUtils::pixelCode2String(configs[i].format),
                configs[i].width, configs[i].height, configs[i].field);

        stream_t stream = getStreamByConfig(configs[i]);

        // Configure streams
        stream_config_t stream_list;
        stream_t streams[1];
        streams[0] = stream;
        streams[0].memType = V4L2_MEMORY_USERPTR;
        stream_list.num_streams = 1;
        stream_list.streams = streams;
        stream_list.operation_mode = CAMERA_STREAM_CONFIGURATION_MODE_AUTO;
        ret = GCM->configStreams(&stream_list);
        EXPECT_EQ(ret, 0);

        // Get common interface for selected graph config setting
        vector <ConfigMode> configModes;
        int ret = PlatformData::getConfigModesByOperationMode(cameraId, stream_list.operation_mode, configModes);
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(configModes.size() > 0);

        shared_ptr<IGraphConfig> GC = GCM->getGraphConfig(configModes[0]);
        EXPECT_NE(GC, nullptr);

        vector<string> pgNames;
        ret = GC->getPgNames(&pgNames);
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(pgNames.size() > 0);

        vector<IGraphConfig::PipelineConnection> connections;
        ret = GC->pipelineGetInternalConnections(pgNames, connections);
        EXPECT_EQ(ret, 0);
        EXPECT_TRUE(connections.size() > 0);

#ifndef BYPASS_MODE
        for (auto& name : pgNames) {
            ia_isp_bxt_program_group programGroup;
            ret = GC->getProgramGroup(name, &programGroup);
            EXPECT_EQ(ret, 0);
            EXPECT_TRUE(programGroup.kernel_count > 0);
        }
#endif
    }

    ret = camera_hal_deinit();
    EXPECT_EQ(ret, 0);
}


