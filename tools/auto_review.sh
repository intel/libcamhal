#
#  Copyright (C) 2015-2016 Intel Corporation
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

#!/bin/bash

########################################################
# This tool is used make automatic review and fix
# based on current code.
#
# Just run "./auto_review.sh" under this tool directory,
# and then "git status", check the automatic fixes the
# tool made.
#
########################################################
current=$PWD

which autoreconf > /dev/null 2>&1

if [[ $? -ne 0 ]]; then
    echo "Please install autoreconf: apt-get install autoreconf"
    exit 1
fi

clang-tidy -list-checks -checks=-*,intel* | grep "intel" > /dev/null 2>&1

if [[ $? -ne 0 ]]; then
    echo -e "\e[1;41mWARN: clang with intel check is not found, not able to update compilation database. \e[0m"
    echo "Please install clang with Intel checks, RPM download from https://wiki.ith.intel.com/download/attachments/504136550/clang_intel_check-1.0.0-0.x86_64.rpm?api=v2"
    exit 1
fi

localchanges=`git status | egrep '*\.cpp|*\.h|*\.c|*\.cxx' | wc -l`

if [[ $localchanges -gt 0 ]]; then
    echo -e "\e[1;41mWARN: Please commit your local changes firstly, before auto review fixes apply any changes to code. \e[0m"
    exit 1
fi

which bear > /dev/null 2>&1

if [[ $? -ne 0 ]]; then
    echo -e "\e[1;41mWARN: bear is not found, not able to update compilation database. \e[0m"
    echo "Please install clang-tidy with Intel checks, RPM download from https://wiki.ith.intel.com/download/attachments/504136550/clang_intel_check-1.0.0-0.x86_64.rpm?api=v2"
    exit 1
fi

export PATH=/usr/local/clang/bin:$PATH
cd ..
autoreconf --install
./configure --disable-static
make clean
bear make -j
./configure
make clean

# Diable some relatively minor rules for now, will fine-tune later.
run-clang-tidy.py -checks=-*,misc-*,readability-*,-readability-braces-around-statements,-readability-else-after-return,google-runtime-*,-google-runtime-int,google-explicit-constructor,cert-*,cppcoreguidelines-* -fix > $current/autofix.log 2>&1

localchanges=`git status | egrep '*\.cpp|*\.h|*\.c|*\.cxx' | wc -l`

if [[ $localchanges -gt 0 ]]; then
    echo -e "\e[1;41mNote: Some auto-fixes applied, please double check them with 'git status'. \e[0m"
else
    echo "Note: No auto-fix applied"
fi

cd $current
