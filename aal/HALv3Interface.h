/*
 * Copyright (C) 2017-2018 Intel Corporation
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

#ifndef _HALV3_INTERFACE_
#define _HALV3_INTERFACE_

namespace camera3 {

/**
 * \brief An interface used to callback RequestManager.
 */
class RequestManagerCallback {
public:
    RequestManagerCallback() {}
    virtual ~RequestManagerCallback() {}

    virtual void returnRequestDone(uint32_t frameNumber) = 0;
};

} // namespace camera3
#endif // _HALV3_INTERFACE_
