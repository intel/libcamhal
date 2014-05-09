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

#include <iomanip>
#include <iostream>
#include <sstream>

#include "ia_pal_types_isp_ids_autogen.h"
#include "ia_pal_types_isp_parameters_autogen.h"

using namespace std;

static const int LENGTH_PER_LINE = 16;

#define SIZE_ALIGN(a,b)            (((unsigned)(a)+(unsigned)(b-1)) & ~(unsigned)(b-1))
#define ALIGNED_SIZE(x)             SIZE_ALIGN(sizeof(x), 8)
#define ARRAY_SIZE(array)          (sizeof(array) / sizeof((array)[0]))
#define PARSE_ONE(field_name) oss << #field_name << ":\t" << field_name << endl;

#define PARSE_ARRAY(filed_name) \\

    oss <<#filed_name << ":\t\tsize:" << ARRAY_SIZE(filed_name) << endl; \\

    for (int i = 0; i < ARRAY_SIZE(filed_name); i++) { \\

        oss << setw(6) << filed_name[i] << " "; \\

        if (i % LENGTH_PER_LINE == LENGTH_PER_LINE - 1) { \\

            oss << endl; \\

        } \\

    } \\

    if (ARRAY_SIZE(filed_name) % LENGTH_PER_LINE != 0) { \\

        oss << endl; \\

    }

% for item in structures:
int parse_isp_${item}(void *data, int size, ostringstream& oss) {
    auto ${item} = reinterpret_cast<ia_pal_isp_${item}_t*>(data);
    oss << "ia_pal_uuid_isp_${item}:" << endl;
    if (size != ALIGNED_SIZE(ia_pal_isp_${item}_t)) {
        cout << "uuid: ia_pal_uuid_isp_${item} cannot be parsed as its size doesn't match..." << endl;
        cout << "Please check if the dump file is matched with headers." << endl;
        return -1;
    }
% for field in structures[item]:
% if field[0]:
    PARSE_ARRAY(${item}->${field[1]});
% else:
    PARSE_ONE(${item}->${field[1]});
% endif
% endfor
    return 0;
}

% endfor
int parseItemByUuid(int uuid, void* data, int size, ostringstream& oss)
{
    switch (uuid)
    {
% for item in structures:
    case ia_pal_uuid_isp_${item}:
        return parse_isp_${item}(data, size, oss);

% endfor
    }
    return 0;
}
