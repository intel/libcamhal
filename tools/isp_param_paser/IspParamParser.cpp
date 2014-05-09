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

#include <iomanip>
#include <iostream>
#include <sstream>
#include <fstream>
#include <map>

#include "IspParamParser.h"
#include "ParseSubItems.h"

#define SIZE_ALIGN(a,b)            (((unsigned)(a)+(unsigned)(b-1)) & ~(unsigned)(b-1))
#define ALIGNED_SIZE(x)             SIZE_ALIGN(sizeof(x), 8)

using namespace std;

IspParamParser::IspParamParser(const char* inputFileName, const char* outputFileName) :
    mInputFile(inputFileName), mOutputFile(outputFileName)
{
}

IspParamParser::~IspParamParser()
{
    delete[] mRawData.data;
}

int IspParamParser::parseData()
{
    int ret = readBinaryFile();
    if (ret != 0) return ret;

    char* pData = mRawData.data;
    int offset = 0;
    RecordHeader* headerPtr = nullptr;
    map<int, string> parsedData;

    while (offset < mRawData.size) {
        headerPtr = reinterpret_cast<RecordHeader*>(pData + offset);
        int uuid = headerPtr->uuid;
        int size = headerPtr->size;

        ostringstream oss;
        oss << "uuid:" << uuid << "\tsize:" << size << endl;
        cout << oss.str();

        char* offsetPtr = pData + offset + ALIGNED_SIZE(RecordHeader);
        int ret = parseItemByUuid(uuid, offsetPtr, size - ALIGNED_SIZE(RecordHeader), oss);
        if (ret != 0) {
              return -1;
        }
        oss << endl << endl;

        parsedData[uuid] = oss.str();
        offset += size;
    }

    ofstream outfile(mOutputFile.c_str());
    for (auto item : parsedData) {
        outfile << item.second;
    }

    return 0;
}

int IspParamParser::readBinaryFile() {
    if (mInputFile.empty()) {
        cout << "Invalid file name" << endl;
    }

    ifstream file(mInputFile.c_str(), ios::in | ios::binary | ios::ate);
    if (!file.is_open()) {
        cout << "Unable to open file:" << mInputFile << endl;
        return -1;
    }

    mRawData.size = static_cast<int>(file.tellg());
    mRawData.data = new char[mRawData.size];
    file.seekg(0, ios::beg);
    file.read(mRawData.data, mRawData.size);

    cout << "Read file:" << mInputFile <<" succeeded" << endl;

    return 0;
}
