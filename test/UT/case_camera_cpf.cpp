/*
 * Copyright (C) 2016-2018 Intel Corporation.
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

#define LOG_TAG "CASE_CPF"

#include "iutils/CameraLog.h"
#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "MockSysCall.h"
#include "CameraConf.h"
#include "PlatformData.h"

using namespace icamera;

/**
 * Test load CPF for sensor enableAIQ
 */
TEST(cameraCpfTest, cpf_load_normal) {
    int camNum = PlatformData::numberOfCameras();
    for (int cameraId = 0; cameraId < camNum; cameraId++) {
        vector <TuningConfig> configs;
        PlatformData::getSupportedTuningConfig(cameraId, configs);
        if (configs.empty()) continue;

        CpfStore cpf(cameraId, PlatformData::getSensorName(cameraId));

        for (auto &cfg : configs) {
            EXPECT_FALSE(cpf.mCpfConfig[cfg.tuningMode] == nullptr);
            if (cpf.mCpfConfig[cfg.tuningMode] == nullptr) {
                continue;
            }

            ia_cmc_t *cmc = cpf.mCpfConfig[cfg.tuningMode]->getCMCHandler();
            EXPECT_FALSE(cmc == nullptr);
            if (cmc == nullptr) {
                continue;
            }

            LOGD("Camera Id %d %s", cameraId, PlatformData::getSensorName(cameraId));
            if (cmc->cmc_general_data != nullptr) {
                LOGD("resolution=[%dx%d], bit depth=[%dx%d]",
                      cmc->cmc_general_data->width,
                      cmc->cmc_general_data->height,
                      cmc->cmc_general_data->bit_depth,
                      cmc->cmc_general_data->single_exposure_bit_depth);
                EXPECT_GT(cmc->cmc_general_data->width, 0);
                EXPECT_GT(cmc->cmc_general_data->height, 0);
            }
            if (cmc->cmc_sensitivity != nullptr) {
                LOGD("base ISO=%d", cmc->cmc_sensitivity->base_iso);
                EXPECT_GT(cmc->cmc_sensitivity->base_iso, 0);
            }
            if (cmc->cmc_parsed_lens_shading.cmc_lens_shading != nullptr) {
                LOGD("lens shading [%dx%d]",
                      cmc->cmc_parsed_lens_shading.cmc_lens_shading->grid_width,
                      cmc->cmc_parsed_lens_shading.cmc_lens_shading->grid_height);
                EXPECT_GT(cmc->cmc_parsed_lens_shading.cmc_lens_shading->grid_width, 0);
                EXPECT_GT(cmc->cmc_parsed_lens_shading.cmc_lens_shading->grid_height, 0);
            }
        }
    }
    PlatformData::releaseInstance();
}
