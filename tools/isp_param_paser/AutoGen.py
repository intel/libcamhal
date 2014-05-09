#!/usr/bin/python

#
#  Copyright (C) 2016-2018 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#

import re
from mako.template import Template


def fill_all_structures(full_text):
    structures = dict()
    start_pos = full_text.find('typedef')
    while start_pos != -1:
        curly_brace_pos = full_text.find('}', start_pos)
        end_pos = full_text.find(';', curly_brace_pos)
        struct_name = full_text[curly_brace_pos+1:end_pos]
        struct_name = struct_name[12:-2]  # remove prefix ia_pal_isp_ and suffix _t
        if struct_name != '':
            structures[struct_name] = full_text[start_pos:end_pos]
        start_pos = full_text.find('typedef', end_pos)

    return structures


def fill_all_field_for_one_structure(structure):
    result = list()
    for line in structure.splitlines():
        if '{' in line or '}' in line or 'typedef' in line:
            continue
        start_pos = line.find(' ')
        end_pos = line.find(';')
        field = line[start_pos+1:end_pos]
        tmp_pos = field.find('[')
        if tmp_pos != -1:
            field_with_type = (True, field[0:tmp_pos])
        else:
            field_with_type = (False, field)
        result.append(field_with_type)

    return result

def generate_cpp_files():
    f = open('./ia_pal_types_isp_parameters_autogen.h', 'r')
    pal_header = f.read()
    pal_header = re.sub('/\*.*\*/', '', pal_header)   # remove comments
    pal_header = re.sub('\n\s*\n*', '\n', pal_header) # remove blank lines

    all_structures = fill_all_structures(pal_header)
    all_items = dict()
    for one_structure in all_structures:
        fields_in_structure = fill_all_field_for_one_structure(all_structures[one_structure])
        all_items[one_structure] = fields_in_structure

    header = Template(filename='ParseSubItemsHeader.mako')
    cpp = Template(filename='ParseSubItemsCpp.mako')

    output_header = open('ParseSubItems.h', 'w')
    output_impl = open('ParseSubItems.cpp', 'w')

    output_header.write(header.render(structures=all_items))
    output_impl.write(cpp.render(structures=all_items))

    f.close()


if __name__ == '__main__':
    generate_cpp_files()

