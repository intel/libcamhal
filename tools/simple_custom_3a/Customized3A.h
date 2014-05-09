/*
 * Copyright (C) 2016-2017 Intel Corporation.
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

#ifndef _CUSTOMIZED_3A_H_
#define _CUSTOMIZED_3A_H_

#include "ia_aiq.h"
#include "ia_isp_types.h"
#include "ia_ltm.h"

#include "Customized3ATypes.h"
#include "Customized3AModule.h"

namespace icamera {

/*
 * \struct GbceResults
 *
 * This struct is used as a wrapper around ia_aiq_gbce_results.
 */
struct GbceResults {
    ia_aiq_gbce_results mGbceResults;
    void reset() {
        memset(&mGbceResults, 0, sizeof(mGbceResults));
    }

    void initGammaLut(unsigned int size) {
        delete mGbceResults.r_gamma_lut;
        delete mGbceResults.g_gamma_lut;
        delete mGbceResults.b_gamma_lut;
        mGbceResults.r_gamma_lut = new float[size];
        mGbceResults.g_gamma_lut = new float[size];
        mGbceResults.b_gamma_lut = new float[size];
        mGbceResults.gamma_lut_size = size;
    }

    void initToneMapLut(unsigned int size) {
        delete mGbceResults.tone_map_lut;
        mGbceResults.r_gamma_lut = new float[size];
        mGbceResults.tone_map_lut_size = size;
    }

    GbceResults() { reset(); }

    ~GbceResults() {
        delete [] mGbceResults.r_gamma_lut;
        delete [] mGbceResults.g_gamma_lut;
        delete [] mGbceResults.b_gamma_lut;
        delete [] mGbceResults.tone_map_lut;
    }
};

/*
 * \struct IspStatistics
 *
 * This struct is used as a wrapper around ia_aiq_statistics_input_params.
 */
struct IspStatistics {
    ia_aiq_hdr_rgbs_grid mHdrRgbsGrid;
    ia_aiq_color_channels mColorGains;

    void reset() {
        memset(&mHdrRgbsGrid, 0, sizeof(mHdrRgbsGrid));
        memset(&mColorGains, 0, sizeof(mColorGains));
    }

    int initHdrRgbsGrid(const ia_aiq_hdr_rgbs_grid *hdrRgbsGrid);

    IspStatistics() { reset(); }

    ~IspStatistics() {
        delete [] mHdrRgbsGrid.blocks_ptr;
    }
};

// init / deinit
int custom3AInit();
int custom3ADeinit();

// set sensor info
int custom3ASetSensorInfo(const ia_aiq_exposure_sensor_descriptor& descriptor);

// update 3a parameters
int custom3AUpdate3AParameters(const custom_3a_parameter_t& param);

// set statistics
int custom3ASetStatistics(const ia_aiq_statistics_input_params_v4 *ispStatistics);

// set GBCE results
int custom3ASetGbceResults(const ia_aiq_gbce_results *gbceResults);

// set LTM parameters
int custom3ASetLtmParams(const ia_ltm_input_params *ltmInputParams, const ia_ltm_drc_params *drcParams);

// calculate post gamma histogram
int calcPostGammaHistogram(const IspStatistics *ispStatistics,
                           const ia_aiq_gbce_results *gbceResults,
                           const ia_ltm_input_params *ltmInputParams,
                           const ia_ltm_drc_params *drcParams);

// ae, af, awb interfaces
int custom3ARunAe(ia_aiq_ae_results *aeResults);
int custom3ARunAf(ia_aiq_af_results *afResults);
int custom3ARunAwb(ia_aiq_awb_results *awbResults);

} // namespace icamera

#endif /* _CUSTOMIZED_3A_H_ */
