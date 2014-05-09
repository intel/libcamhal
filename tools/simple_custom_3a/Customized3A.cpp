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

#define LOG_TAG "Customized3A"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <memory.h>
#include <cmath>

#include "Customized3A.h"
#include "Customized3AModule.h"

namespace icamera {

#ifdef LOG3A
#undef LOG3A
#endif

#define LOG3A(format, args...) printLog(format, ##args)

#ifndef MEMCPY_S
#define MEMCPY_S(dest, dmax, src, smax) memcpy((dest), (src), std::min((size_t)(dmax), (size_t)(smax)))
#endif

#define BPP                         16
#define MAX_PIX_VAL                 ((1 << (BPP - 1)) - 1)
#define MIN_PIX_VAL                 ((-1) * (1 << (BPP - 1)))
#define GAIN_FRAQ_BITS              6
#define MIN(a, b)                   ((a) < (b) ? (a) : (b))
#define MAX(a, b)                   ((a) > (b) ? (a) : (b))
/* Does a right shift (division) rounding to the closest integer. */
#define ROUND_RSHIFT(x, SHIFT_BITS) (((SHIFT_BITS) == 0) ? (x) : (((x) + (1 << ((SHIFT_BITS) - 1))) >> (SHIFT_BITS)))

static GbceResults   *gGbceResults   = nullptr;
static IspStatistics *gIspStatistics = nullptr;

static int gLogLevel = 0;
static void setDebugLevel(void)
{
    const char* PROP_CUSTOM_3A_DEBUG = "custom3ADebug";

    char *dbgLevel = getenv(PROP_CUSTOM_3A_DEBUG);
    if (dbgLevel != NULL) {
        gLogLevel = strtoul(dbgLevel, NULL, 0);
    }
}

static void printLog(const char *format, ...)
{
    if (gLogLevel == 0) return;

    va_list arg;
    va_start(arg, format);

    vfprintf(stdout, format, arg);
    va_end(arg);
}

static void writeData(const void* data, int size, const char* fileName)
{
    if (data == NULL || size == 0 || fileName == NULL) {
        LOG3A("Nothing needs to be dumped \n");
        return;
    }

    FILE *fp = fopen (fileName, "w+");
    if (fp == NULL) {
        LOG3A("open dump file %s failed \n", fileName);
        return;
    }

    LOG3A("Write data to file:%s \n", fileName);
    if ((fwrite(data, size, 1, fp)) != 1)
        LOG3A("Error or short count writing %d bytes to %s \n", size, fileName);

    fclose (fp);
}

/**
 * \macro VISIBILITY_PUBLIC
 *
 * Controls the visibility of symbols in the shared library.
 * In production builds all symbols in the shared library are hidden
 * except the ones using this linker attribute.
 */
#define VISIBILITY_PUBLIC __attribute__ ((visibility ("default")))

extern "C" Custom3AModule VISIBILITY_PUBLIC CUSTOMIZE_3A_MODULE_INFO_SYM = {
    .custom3AModuleVersion = 1,
    .custom3ACapability = CUSTOM_AE | CUSTOM_AWB,
    .init = custom3AInit,
    .deinit = custom3ADeinit,
    .setSensorInfo = custom3ASetSensorInfo,
    .updateParameter = custom3AUpdate3AParameters,
    .setStatistics = custom3ASetStatistics,
    .setGbceResults = custom3ASetGbceResults,
    .setLtmParams = custom3ASetLtmParams,
    .runAe = custom3ARunAe,
    .runAf = custom3ARunAf,
    .runAwb = custom3ARunAwb
};

static const int MAX_NAME_LEN = 256;

static int g_custom_3a_index = -1;
static const int MAX_AE_LOOP_NUM = 30;
static const int MAX_AWB_LOOP_NUM = 30;

int IspStatistics::initHdrRgbsGrid(const ia_aiq_hdr_rgbs_grid *hdrRgbsGrid)
{
   if (hdrRgbsGrid == nullptr) {
       LOG3A("Incorrect HDR RGBS grid.");
       return -1;
   }

   if (mHdrRgbsGrid.grid_width != hdrRgbsGrid->grid_width ||
       mHdrRgbsGrid.grid_height != hdrRgbsGrid->grid_height) {
           delete [] mHdrRgbsGrid.blocks_ptr;
           mHdrRgbsGrid.blocks_ptr = nullptr;
   }

   mHdrRgbsGrid.grid_width = hdrRgbsGrid->grid_width;
   mHdrRgbsGrid.grid_height = hdrRgbsGrid->grid_height;
   mHdrRgbsGrid.grid_data_bit_depth = hdrRgbsGrid->grid_data_bit_depth;
   mHdrRgbsGrid.shading_correction = hdrRgbsGrid->shading_correction;

   if (mHdrRgbsGrid.blocks_ptr == nullptr) {
       mHdrRgbsGrid.blocks_ptr = new hdr_rgbs_grid_block[mHdrRgbsGrid.grid_width * mHdrRgbsGrid.grid_height];
   }

   for (int i = 0; i < mHdrRgbsGrid.grid_height * mHdrRgbsGrid.grid_width; i++) {
       mHdrRgbsGrid.blocks_ptr[i] = hdrRgbsGrid->blocks_ptr[i];
   }
}

int custom3AInit()
{
    // init debug log level
    setDebugLevel();

    LOG3A("enter custom 3a init \n");
    g_custom_3a_index = -1;

    gGbceResults = new GbceResults;

    gIspStatistics = new IspStatistics;

    return 0;
}

int custom3ADeinit()
{
    LOG3A("enter custom 3a deinit \n");
    g_custom_3a_index = -1;

    delete gGbceResults;
    gGbceResults = nullptr;

    delete gIspStatistics;
    gIspStatistics = nullptr;

    return 0;
}

int custom3ASetSensorInfo(const ia_aiq_exposure_sensor_descriptor& descriptor)
{
    LOG3A("enter custom 3a setSensorInfo \n");

    return 0;
}

int custom3AUpdate3AParameters(const custom_3a_parameter_t& param)
{
    LOG3A("enter custom 3a updateParameter \n");

    LOG3A("Application parameters: \n");
    LOG3A("ae mode:%d, awb mode:%d scene mode:%d \n", param.aeMode, param.awbMode, param.sceneMode);
    LOG3A("EV:%f, manualExpTimeUs:%ld, manualGain:%f \n", param.evShift, param.manualExpTimeUs, param.manualGain);
    LOG3A("FPS:%d \n", param.fps);
    LOG3A("Antibanding mode:%d \n", param.antibandingMode);
    LOG3A("cctRange:(%d-%d) \n", param.cctRange.min, param.cctRange.max);
    LOG3A("manual white point:(%d,%d) \n", param.whitePoint.x, param.whitePoint.y);
    LOG3A("manual awb gain:(%d,%d,%d) \n", param.awbManualGain.r_gain, param.awbManualGain.g_gain, param.awbManualGain.b_gain);
    LOG3A("manual awb gain shift:(%d,%d,%d) \n", param.awbGainShift.r_gain, param.awbGainShift.g_gain, param.awbGainShift.b_gain);
    for (int i = 0; i < 3; i++) {
        LOG3A("manual color matrix:  [%.3f %.3f %.3f] \n",
            param.manualColorMatrix.color_transform[i][0],
            param.manualColorMatrix.color_transform[i][1],
            param.manualColorMatrix.color_transform[i][2]);
    }
    LOG3A("manual color gains in rggb:(%d,%d,%d,%d) \n",
        param.manualColorGains.color_gains_rggb[0], param.manualColorGains.color_gains_rggb[1],
        param.manualColorGains.color_gains_rggb[2], param.manualColorGains.color_gains_rggb[3]);
    LOG3A("ae region size:%zu, blc area mode:%d \n", param.aeRegions.size(), param.blcAreaMode);
    for (auto &region : param.aeRegions) {
        LOG3A("ae region (%d, %d, %d, %d, %d) \n",
            region.left, region.top, region.right, region.bottom, region.weight);
    }
    LOG3A("ae converge speed mode:(%d) awb converge speed mode:(%d) \n", param.aeConvergeSpeedMode, param.awbConvergeSpeedMode);
    LOG3A("ae converge speed:(%d) awb converge speed:(%d) \n", param.aeConvergeSpeed, param.awbConvergeSpeed);
    LOG3A("HDR Level:(%d) \n", param.hdrLevel);
    LOG3A("weight grid mode:%d \n", param.weightGridMode);
    LOG3A("AE Distribution Priority:%d \n", param.aeDistributionPriority);

    return 0;
}

short calcGtm(unsigned short x, const ia_ltm_drc_gtm *gtm)
{
    short y = 1 << gtm->gtm_gain_frac_bit;

    if (!gtm->gtm_bypass)
    {
        unsigned short a = MAX(x, 1);
        int idx;
        for (idx = -1; a != 0; idx++) a >>= 1;
        unsigned short offset = gtm->xcu_gtm_offset_vec[idx];
        short slope = gtm->xcu_gtm_slope_vec[idx];
        unsigned short x_val_prev = gtm->xcu_gtm_x_cord_vec[idx];
        y = ROUND_RSHIFT(slope * (x - x_val_prev), gtm->xcu_gtm_slope_resolution);
        y >>= MAX(idx - gtm->gtm_gain_frac_bit - 1, 0);
        y += offset;
    }

    return y;
}

int calcPostGammaHistogram(const IspStatistics *ispStatistics,
                           const ia_aiq_gbce_results *gbceResults,
                           const ia_ltm_input_params *ltmInputParams,
                           const ia_ltm_drc_params *drcParams)
{
    LOG3A("@%s", __func__);

    const ia_aiq_hdr_rgbs_grid *hdr_rgbs_grid = &ispStatistics->mHdrRgbsGrid;
    const ia_aiq_color_channels *color_gains = &ispStatistics->mColorGains;

    const float v_ratio = (float)ltmInputParams->yv_grid->grid_height / hdr_rgbs_grid->grid_height;
    const float h_ratio = (float)ltmInputParams->yv_grid->grid_width  / hdr_rgbs_grid->grid_width;
    const int num_bins = 256;
    unsigned int gamma_lut_size = gbceResults->gamma_lut_size;
    int cc_r, cc_g, cc_b;
    int gamma_r, gamma_g, gamma_b;
    int reduce_bits = 2;
    int pre_gamma_reduce_bits = 5;

    ia_aiq_histogram hist = { 0 };
    // Init
    hist.r = new unsigned int[num_bins]();
    hist.g = new unsigned int[num_bins]();
    hist.b = new unsigned int[num_bins]();
    hist.num_r_elements = num_bins;
    hist.num_g_elements = num_bins;
    hist.num_b_elements = num_bins;

    for (int row = 0; row < hdr_rgbs_grid->grid_height; row++)
    {
        for (int col = 0; col < hdr_rgbs_grid->grid_width; col++)
        {
            hdr_rgbs_grid_block *grid_block = &hdr_rgbs_grid->blocks_ptr[row * hdr_rgbs_grid->grid_width + col];

            unsigned int r = grid_block->avg_r;
            unsigned int g = (grid_block->avg_gr + grid_block->avg_gb + 0.5) / 2;
            unsigned int b = grid_block->avg_b;
            unsigned int max = MAX(MAX(r, g), b);
            unsigned short gtm_gain_frac_bit = drcParams->drc_gtm.gtm_gain_frac_bit;

            // local tonemap
            int gain_map_idx = int(std::round(row * v_ratio)) * ltmInputParams->yv_grid->grid_width +
                               int(std::round(col * h_ratio));
            gain_map_idx = MIN(gain_map_idx, ltmInputParams->yv_grid->grid_width * ltmInputParams->yv_grid->grid_height);
            r = ROUND_RSHIFT(r * drcParams->gain_map[gain_map_idx] * calcGtm(max, &drcParams->drc_gtm), GAIN_FRAQ_BITS + gtm_gain_frac_bit);
            g = ROUND_RSHIFT(g * drcParams->gain_map[gain_map_idx] * calcGtm(max, &drcParams->drc_gtm), GAIN_FRAQ_BITS + gtm_gain_frac_bit);
            b = ROUND_RSHIFT(b * drcParams->gain_map[gain_map_idx] * calcGtm(max, &drcParams->drc_gtm), GAIN_FRAQ_BITS + gtm_gain_frac_bit);

            int cc_r = r * color_gains->r;
            int cc_g = g * (color_gains->gr + color_gains->gb) / 2;
            int cc_b = b * color_gains->b;

            r = MAX(MIN(gamma_lut_size - 1, ROUND_RSHIFT(cc_r, pre_gamma_reduce_bits)), 0);
            g = MAX(MIN(gamma_lut_size - 1, ROUND_RSHIFT(cc_g, pre_gamma_reduce_bits)), 0);
            b = MAX(MIN(gamma_lut_size - 1, ROUND_RSHIFT(cc_b, pre_gamma_reduce_bits)), 0);

            // gamma correction
            r = (int)((num_bins - 1) * gbceResults->r_gamma_lut[r] + 0.5f);
            g = (int)((num_bins - 1) * gbceResults->g_gamma_lut[g] + 0.5f);
            b = (int)((num_bins - 1) * gbceResults->b_gamma_lut[b] + 0.5f);

            r = MIN(num_bins - 1, r);
            g = MIN(num_bins - 1, g);
            b = MIN(num_bins - 1, b);

            hist.r[r]++;
            hist.g[g]++;
            hist.b[b]++;
        }
    }

    if (g_custom_3a_index == 30) {
        char fileName[MAX_NAME_LEN] = {'\0'};
        snprintf(fileName, (MAX_NAME_LEN-1), "post-gamma-hist_r.bin");
        writeData(hist.r, hist.num_r_elements * sizeof(*hist.r), fileName);
        snprintf(fileName, (MAX_NAME_LEN-1), "post-gamma-hist_g.bin");
        writeData(hist.g, hist.num_g_elements * sizeof(*hist.g), fileName);
        snprintf(fileName, (MAX_NAME_LEN-1), "post-gamma-hist_b.bin");
        writeData(hist.b, hist.num_b_elements * sizeof(*hist.b), fileName);
    }

    delete [] hist.r;
    delete [] hist.g;
    delete [] hist.b;

    return 0;
}

int custom3ASetStatistics(const ia_aiq_statistics_input_params_v4 *ispStatistics)
{
    LOG3A("enter custom 3a setStatistics \n");

    g_custom_3a_index++;

    if (ispStatistics == NULL
        || gIspStatistics == NULL
        || ispStatistics->num_rgbs_grids == 0
        || ispStatistics->rgbs_grids == NULL) {
        return -1;
    }

    gIspStatistics->initHdrRgbsGrid(ispStatistics->hdr_rgbs_grid);

    const ia_aiq_rgbs_grid *rgbsGrid = *ispStatistics->rgbs_grids;

    for (unsigned int i = 0; i < ispStatistics->num_rgbs_grids; i++ ) {
        rgbs_grid_block *rgbsPtr = rgbsGrid[i].blocks_ptr;
        int size = rgbsGrid[i].grid_width * rgbsGrid[i].grid_height;

        int sumLuma = 0;
        for (int j = 0; j < size; j++) {
            sumLuma += (rgbsPtr[j].avg_b + rgbsPtr[j].avg_r + (rgbsPtr[j].avg_gb + rgbsPtr[j].avg_gr) / 2) / 3;
        }
        LOG3A("custom RGB stat grid[%d] %dx%d, y_mean %d \n", i, rgbsGrid[i].grid_width, rgbsGrid[i].grid_height, sumLuma/size);

        if (g_custom_3a_index == 30) {
            char fileName[MAX_NAME_LEN] = {'\0'};
            snprintf(fileName, (MAX_NAME_LEN-1), "ia_aiq_statistics_num_%d_ymean_%d_id_%d.bin",
                      g_custom_3a_index, sumLuma/size, i);
            writeData(rgbsGrid[i].blocks_ptr, rgbsGrid[i].grid_width * rgbsGrid[i].grid_height * sizeof(rgbs_grid_block), fileName);
        }
    }

    return 0;
}

int custom3ASetGbceResults(const ia_aiq_gbce_results *gbceResults)
{
    LOG3A("@%s", __func__);

    // Copy GBCE results
    if (gGbceResults == nullptr) {
        LOG3A("Invalid destination");
        return -1;
    }

    if (gbceResults->gamma_lut_size != gGbceResults->mGbceResults.gamma_lut_size) {
        gGbceResults->initGammaLut(gbceResults->gamma_lut_size);
    }

    if (gbceResults->tone_map_lut_size != gGbceResults->mGbceResults.tone_map_lut_size) {
        gGbceResults->initToneMapLut(gbceResults->tone_map_lut_size);
    }

    MEMCPY_S(gGbceResults->mGbceResults.r_gamma_lut, gbceResults->gamma_lut_size * sizeof(float),
             gbceResults->r_gamma_lut, gbceResults->gamma_lut_size * sizeof(float));

    MEMCPY_S(gGbceResults->mGbceResults.g_gamma_lut, gbceResults->gamma_lut_size * sizeof(float),
             gbceResults->g_gamma_lut, gbceResults->gamma_lut_size * sizeof(float));

    MEMCPY_S(gGbceResults->mGbceResults.b_gamma_lut, gbceResults->gamma_lut_size * sizeof(float),
             gbceResults->b_gamma_lut, gbceResults->gamma_lut_size * sizeof(float));

    // Copy tone mapping table
    MEMCPY_S(gGbceResults->mGbceResults.tone_map_lut, gbceResults->tone_map_lut_size * sizeof(float),
             gbceResults->tone_map_lut, gbceResults->tone_map_lut_size * sizeof(float));

    return 0;
}

int custom3ASetLtmParams(const ia_ltm_input_params *ltmInputParams, const ia_ltm_drc_params *drcParams)
{
    LOG3A("@%s", __func__);

    calcPostGammaHistogram(gIspStatistics, &gGbceResults->mGbceResults, ltmInputParams, drcParams);

    return 0;
}

ia_aiq_exposure_parameters exposure_parameters[MAX_AE_LOOP_NUM] =
{
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},

    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},

    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},
    {1877, 3.7, 1.0, 1.4, 6974, false, 167},

};
ia_aiq_exposure_sensor_parameters sensor_exposure_parameters[MAX_AE_LOOP_NUM] =
{
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},
    {0, 10, 10, 256, 2200, 1135},

    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},
    {0, 30, 20, 256, 2200, 1135},

    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},
    {0, 50, 30, 256, 2200, 1135},

};

int custom3ARunAe(ia_aiq_ae_results *aeResults)
{
    LOG3A("enter custom 3a runAe \n");

    int ae_index = g_custom_3a_index % MAX_AE_LOOP_NUM;

    if (aeResults == NULL || aeResults->exposures == NULL
        || aeResults->exposures->exposure == NULL
        || aeResults->exposures->sensor_exposure == NULL) {
        return -1;
    }

    aeResults->exposures->exposure->exposure_time_us = exposure_parameters[ae_index].exposure_time_us;
    aeResults->exposures->exposure->analog_gain = exposure_parameters[ae_index].analog_gain;
    aeResults->exposures->exposure->digital_gain = exposure_parameters[ae_index].digital_gain;
    aeResults->exposures->exposure->aperture_fn = exposure_parameters[ae_index].aperture_fn;
    aeResults->exposures->exposure->total_target_exposure = exposure_parameters[ae_index].total_target_exposure;
    aeResults->exposures->exposure->nd_filter_enabled = exposure_parameters[ae_index].nd_filter_enabled;
    aeResults->exposures->exposure->iso = exposure_parameters[ae_index].iso;

    aeResults->exposures->sensor_exposure->fine_integration_time = sensor_exposure_parameters[ae_index].fine_integration_time;
    aeResults->exposures->sensor_exposure->coarse_integration_time = sensor_exposure_parameters[ae_index].coarse_integration_time;
    aeResults->exposures->sensor_exposure->analog_gain_code_global = sensor_exposure_parameters[ae_index].analog_gain_code_global;
    aeResults->exposures->sensor_exposure->digital_gain_global = sensor_exposure_parameters[ae_index].digital_gain_global;
    aeResults->exposures->sensor_exposure->line_length_pixels = sensor_exposure_parameters[ae_index].line_length_pixels;
    aeResults->exposures->sensor_exposure->frame_length_lines = sensor_exposure_parameters[ae_index].frame_length_lines;

    aeResults->exposures->exposure_index = g_custom_3a_index;
    aeResults->exposures->distance_from_convergence = 0;
    aeResults->exposures->converged = true;
    aeResults->exposures->num_exposure_plan = 1;

    if (aeResults->aperture_control != NULL) {
        aeResults->aperture_control->aperture_fn = -1;
        aeResults->aperture_control->dc_iris_command = ia_aiq_aperture_control_dc_iris_open;
        aeResults->aperture_control->code = 1000;
    }

    aeResults->num_exposures = 1;
    aeResults->num_flashes = 0;
    aeResults->multiframe = ia_aiq_bracket_mode_none;
    aeResults->flicker_reduction_mode = ia_aiq_ae_flicker_reduction_50hz;

    return 0;
}

int custom3ARunAf(ia_aiq_af_results *afResults)
{
    LOG3A("enter custom 3a runAf \n");

    return 0;
}

ia_aiq_awb_results awb_result[MAX_AWB_LOOP_NUM] =
{
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},
    {0.116666, 0.514764, 0.116666, 0.514764, 4808, 0.000000},

    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},
    {0.216666, 0.514764, 0.216666, 0.514764, 4808, 0.000000},

    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
    {0.416666, 0.514764, 0.416666, 0.514764, 4808, 0.000000},
};

int custom3ARunAwb(ia_aiq_awb_results *awbResults)
{
    LOG3A("enter custom 3a runAwb \n");

    int awb_index = g_custom_3a_index % MAX_AWB_LOOP_NUM;

    if (awbResults == NULL) {
        return -1;
    }

    awbResults->accurate_r_per_g = awb_result[awb_index].accurate_r_per_g;
    awbResults->accurate_b_per_g = awb_result[awb_index].accurate_b_per_g;
    awbResults->final_r_per_g = awb_result[awb_index].final_r_per_g;
    awbResults->final_b_per_g = awb_result[awb_index].final_b_per_g;
    awbResults->cct_estimate = awb_result[awb_index].cct_estimate;
    awbResults->distance_from_convergence = awb_result[awb_index].distance_from_convergence;

    return 0;
}

} // namespace icamera
