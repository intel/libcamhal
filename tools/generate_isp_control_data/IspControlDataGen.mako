/*
 * Copyright (C) 2017 Intel Corporation.
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
#include <string.h>

% if len(data_list) != 0:
#include "IspControl.h"
using namespace icamera;
% endif

% if ltm_tuning:
#include "AlgoTuning.h"
% endif

using namespace std;

#define CLEAR(x) memset (&(x), 0, sizeof (x))
#define ALIGN(val, alignment) (((val)+(alignment)-1) & ~((alignment)-1))
#define ALIGN_8(val)  ALIGN(val, 8)

struct RecordHeader {
    unsigned int uuid;
    unsigned int size;
};

% for item in data_list:
int fill_data_${item.base_name}(char *data) {
    const size_t kHeaderSize = sizeof(RecordHeader);
    RecordHeader* header = (RecordHeader*)data;
    header->uuid = camera_control_isp_ctrl_id_${item.base_name};
    header->size = ALIGN_8(kHeaderSize + sizeof(camera_control_isp_${item.base_name}_t));

    camera_control_isp_${item.base_name}_t ${item.base_name};
    CLEAR(${item.base_name});

    % for k, v in item.members.items():
      % if '{' in v and '}' in v:
    {
        double tmp[] = ${v};
        for (int i = 0; i < sizeof(tmp) / sizeof(tmp[0]); i++) {
            ${item.base_name}.${k}[i] = tmp[i];
        }
    }
      % else:
    ${item.base_name}.${k} = ${v};
      % endif
    % endfor

    memcpy(data + kHeaderSize, &${item.base_name}, sizeof(camera_control_isp_${item.base_name}_t));

    return header->size;
}

% endfor

% if ltm_tuning:
int fill_data_ltm_tuning_data(char *data) {
    ltm_tuning_data ${ltm_tuning.base_name};
    CLEAR(${ltm_tuning.base_name});

    % for k, v in ltm_tuning.members.items():
      % if '{' in v and '}' in v:
    {
        double tmp[] = ${v};
        for (int i = 0; i < sizeof(tmp) / sizeof(float); i++) {
            ${ltm_tuning.base_name}.${k}[i] = tmp[i];
        }
    }
      % else:
    ${ltm_tuning.base_name}.${k} = ${v};
      % endif
    % endfor

    memcpy(data, &${ltm_tuning.base_name}, sizeof(ltm_tuning_data));

    return sizeof(ltm_tuning_data);
}
% endif

int main(int argc, char **argv)
{
    const int kMaxSize = 1024 * 1024;
    char data[kMaxSize] = {0};
    int totalSize = 0;

% for item in data_list:
    totalSize += fill_data_${item.base_name}(data + totalSize);
    if (totalSize > kMaxSize) {
        cout << "Exceeding the max buffer size:" << kMaxSize << endl;
        return -1;
    }
% endfor

% if ltm_tuning:
    totalSize = fill_data_ltm_tuning_data(data);
% endif

    const char* fileName = argv[1];
    if (fileName == NULL) fileName = "default_output.bin";
    FILE *fp = fopen (fileName, "w+");
    if (fp == NULL) {
        cout << "Open output file: " << fileName << " failed" << endl;
        return -1;
    }

    if ((fwrite(data, totalSize, 1, fp)) != 1) {
        cout << "Failed to write data to: " << fileName << endl;
    }
    fclose (fp);

    return 0;
}
