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

#pragma once

#include <string>
#include <sstream>

#include "ia_pal_types_isp.h"

typedef ia_pal_record_header RecordHeader;


struct RawBinaryData {
    RawBinaryData() {
        size = 0;
        data = nullptr;
    }
    int size;
    char* data;
};

class IspParamParser {
public:
    IspParamParser(const char* inputFileName, const char* outputFileName);
    ~IspParamParser();
    int parseData();

    // Copy construct and assignment operator is not allowed
    IspParamParser(const IspParamParser& rhs) = delete;
    IspParamParser& operator = (const IspParamParser& rhs) = delete;

private:
    int readBinaryFile();

private:
    std::string mInputFile;
    std::string mOutputFile;
    RawBinaryData mRawData;
};
