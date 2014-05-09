/*
 * Copyright (C) 2015-2016 Intel Corporation.
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

#ifndef _ICUSTOMIZED_AIC_H_
#define _ICUSTOMIZED_AIC_H_

#include "ia_aiq.h"
#include "ia_isp_types.h"

#include "CustomizedAicTypes.h"

/*
 * \interface class ICustomizedAic
 * This is an interface class for customized Aic.
 */
class ICustomizedAic {

public:
    virtual ~ICustomizedAic() {};

    virtual int init() = 0;
    virtual int deinit() = 0;
    virtual int runExternalAic(const ia_aiq_ae_results &ae_results,
                               const ia_aiq_awb_results &awb_results,
                               ia_isp_custom_controls *custom_controls,
                               CustomAicPipe *pipe);
};

#endif /* _ICUSTOMIZED_AIC_H_ */
