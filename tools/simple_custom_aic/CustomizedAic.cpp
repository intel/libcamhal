/*
 * Copyright (C) 2016 Intel Corporation.
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

#define LOG_TAG "CustomizedAic"

#include <stdio.h>
#include <stdarg.h>
#include <memory.h>
#include <string.h>

#include "CustomizedAic.h"
#include "CustomizedAicModule.h"

namespace icamera {

#ifdef LOGAIC
#undef LOGAIC
#endif

#define LOGAIC(format, args...) printLog(format, ##args)

static int gLogLevel = 0;
static void setDebugLevel(void)
{
    const char* PROP_CUSTOM_AIC_DEBUG = "customAicDebug";

    char *dbgLevel = getenv(PROP_CUSTOM_AIC_DEBUG);
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

static const int AIC_PARAM_DATA_MAX = 1024;

static int AicParamDataCount;
static float AicParamData[AIC_PARAM_DATA_MAX];

/**
 * \macro VISIBILITY_PUBLIC
 *
 * Controls the visibility of symbols in the shared library.
 * In production builds all symbols in the shared library are hidden
 * except the ones using this linker attribute.
 */
#define VISIBILITY_PUBLIC __attribute__ ((visibility ("default")))

extern "C" CustomAicModule VISIBILITY_PUBLIC CUSTOMIZE_AIC_MODULE_INFO_SYM = {
    .customAicModuleVersion = 1,
    .init = customAicInit,
    .deinit = customAicDeinit,
    .setAicParam = customAicSetParameters,
    .runExternalAic = customAicRunExternalAic
};

int customAicInit()
{
    // init debug log level
    setDebugLevel();

    LOGAIC("enter custom aic init \n");
    AicParamDataCount = 0;
    memset(AicParamData, 0, sizeof(AicParamData));
    return 0;
}

int customAicDeinit()
{
    LOGAIC("enter custom aic deinit \n");
    return 0;
}

int customAicSetParameters(const CustomAicParam &customAicParam)
{
    LOGAIC("enter custom aic setParameter \n");
    // for simple code, parameter string is passed like "aa,bb,cc,dd,".
    char* srcDup = strdup((char*)customAicParam.data);
    if (srcDup == NULL) return -1;

    char* srcTmp = srcDup;
    char* endPtr = NULL;
    unsigned int index = 0;
    while ((endPtr = (char*)strchr(srcTmp, ','))) {
        *endPtr = 0;
        float param = strtof(srcTmp, &endPtr);
        AicParamData[index] = param;
        index++;
        if (endPtr) {
            srcTmp = endPtr + 1;
        }
    }

    AicParamDataCount = index;

    free(srcDup);

    return 0;
}

int customAicRunExternalAic(const ia_aiq_ae_results &ae_results,
                   const ia_aiq_awb_results &awb_results,
                   ia_isp_custom_controls *custom_controls,
                   CustomAicPipe *pipe)
{
    LOGAIC("enter custom aic runExternalAic \n");
    // below is simple code, please change it based on real configuration
    *pipe = CUSTOM_AIC_PIPE_HDR;

    custom_controls->parameters[0] = 1;
    custom_controls->parameters[1] = 1;
    custom_controls->count = 2;

    // overwrite it if there is Aic parameter setting
    if (AicParamDataCount > 0) {
        unsigned int i = 0;
        for (i = 0; i < AicParamDataCount; i++) {
            custom_controls->parameters[i] = AicParamData[i];
        }

        custom_controls->count = AicParamDataCount;
    }

    return 0;
}

} // namespace icamera
