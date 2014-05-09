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

#include <iostream>
#include <vector>

#include "IspParamParser.h"

using namespace std;

int main(int argc, char **argv)
{
    vector<string> inputFiles;
    if (argc == 1) {
        cout << "No file provided, will use default file: dump.bin" << endl;
        inputFiles.push_back(string("dump.bin"));
    } else {
        for (int i = 1; i < argc; i++) {
            inputFiles.push_back(string(argv[i]));
        }
    }

    for (string file : inputFiles) {
        string output = file + ".out";
        cout << "File:" << file << " is going to be parsed..." << endl;
        cout << "Result will be saved to: " << output << " if succeeded."<< endl;

        IspParamParser parser(file.c_str(), output.c_str());
        int ret = parser.parseData();
        cout << "Parse dump file:" << file << ((ret == 0) ? " succeeded." : " failed.") << endl;
    }

    return 0;
}
