#!/usr/bin/python

#
#  Copyright (C) 2017 Intel Corporation
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

import os.path
import sys
import re

from mako.template import Template

CTRL_ID_PREFIX = 'camera_control_isp_ctrl_id_'
COMMENTS_PREFIX = '//'


class ControlData:
    def __init__(self, full_name):
        self.base_name = full_name[len(CTRL_ID_PREFIX):]
        self.members = dict()

    def add_member(self, k, v):
        self.members[k] = v


def remove_blanks(string):
    return re.sub(r'[\n|\r|\t| ]',r'', string)

def get_ctrl_data_list(data_file):
    enabled_flag = 'feature_enabled'

    with open(data_file, 'rt') as f:
        _ctrl_data_list = list()
        tag_index = -1
        need_skip = False

        for line in f:
            line = remove_blanks(line)
            if CTRL_ID_PREFIX in line:
                _ctrl_data_list.append(ControlData(line))
                tag_index += 1
                need_skip = False
            elif tag_index != -1 and line != '' and not line.startswith(COMMENTS_PREFIX):
                if enabled_flag in line:
                    if '0' in line:
                        need_skip = True
                        del _ctrl_data_list[tag_index]
                        tag_index -= 1
                    elif '1' in line:
                        need_skip = False
                elif not need_skip:
                    key, _, value = line.partition(':')
                    value, _, _ = value.partition(COMMENTS_PREFIX)
                    _ctrl_data_list[tag_index].add_member(key, remove_blanks(value))

        return _ctrl_data_list


def get_ltm_tuning_data(data_file):
    with open(data_file, 'rt') as f:
        _ltm_tuning_data = ControlData('')
        _ltm_tuning_data.base_name = 'ltm_tuning'

        for line in f:
            line = remove_blanks(line)
            if line != '' and not line.startswith(COMMENTS_PREFIX):
                key, _, value = line.partition(':')
                value, _, _ = value.partition(COMMENTS_PREFIX)
                _ltm_tuning_data.add_member(key, remove_blanks(value))

        return _ltm_tuning_data


def fill_ltm_tuning_data(data_file):
    if not data_file:
        return

    if not os.path.exists(data_file):
        print('Invalid input LTM tuning data file:', data_file)
        return

    if os.path.exists('../../include/api/AlgoTuning.h'):
        os.system('cp ../../include/api/AlgoTuning.h .')

    if not os.path.exists('AlgoTuning.h'):
        print('The required AlgoTuning.h is missing, please copy it from libcamhal/include/api/AlgoTuning.h.')
        return

    template = Template(filename='IspControlDataGen.mako')
    temp_cpp = 'main.cpp'
    temp_bin = 'exe'
    output = open(temp_cpp, 'w')
    output.write(template.render(data_list=list(), ltm_tuning=get_ltm_tuning_data(data_file)))
    output.close()

    os.system('rm -f ' + temp_bin)
    os.system('g++ -std=c++11 -o {} {}'.format(temp_bin, temp_cpp))
    if not os.path.exists(temp_bin):
        print('Build error, you can check if there are error in' + temp_cpp)
        return

    os.system('./{} {}.bin'.format(temp_bin, data_file))
    os.system('rm -f {} {}'.format(temp_cpp, temp_bin))

    print('The LTM tuning data file {}.bin was generated...'.format(data_file))


def fill_isp_control_data(data_file):
    if not data_file:
        return

    if not os.path.exists(data_file):
        print('Invalid input ISP control data file:', data_file)
        return

    if os.path.exists('../../include/api/IspControl.h'):
        os.system('cp ../../include/api/IspControl.h .')

    if not os.path.exists('IspControl.h'):
        print('The required IspControl.h is missing, please copy it from libcamhal/include/api/IspControl.h.')
        return

    template = Template(filename='IspControlDataGen.mako')
    temp_cpp = 'main.cpp'
    temp_bin = 'exe'
    output = open(temp_cpp, 'w')
    output.write(template.render(data_list=get_ctrl_data_list(data_file)))
    output.close()

    os.system('rm -f ' + temp_bin)
    os.system('g++ -std=c++11 -o {} {}'.format(temp_bin, temp_cpp))
    if not os.path.exists(temp_bin):
        print('Build error, you can check if there are error in' + temp_cpp)
        return

    os.system('./{} {}.bin'.format(temp_bin, data_file))
    os.system('rm -f {} {}'.format(temp_cpp, temp_bin))

    print('ISP control data file {}.bin was generated...'.format(data_file))


if __name__ == '__main__':
    if len(sys.argv) == 1:
        print('Neither ISP control data file nor LTM tuning data file is provided')
        sys.exit(-1)

    ltm_tuning_prefix = 'ltm_tuning='

    for index in range(1, len(sys.argv)):
        _data_file = sys.argv[index]
        if ltm_tuning_prefix in _data_file:
            _, _, ltm_data_file = _data_file.partition(ltm_tuning_prefix)
            fill_ltm_tuning_data(ltm_data_file)
        else:
            fill_isp_control_data(_data_file)
