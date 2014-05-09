#
#  Copyright (C) 2014-2016 Intel Corporation
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
XRCHost=$1
if [[ -z $XRCHost ]]; then
    echo "Please specify XRC host IP like, 'run_all_mondello_tests.sh xxx.xxx.xxx.xxx'"
    exit 1
else
    export XRCHost
fi
if [[ -z `which unbuffer` ]];then
    echo "Error and Tip: Please 'sudo apt-get install expect-dev' firstly and then run this again."
    exit 1
fi
export cameraDebug=3
export cameraInput=mondello
rm -f ./*.yuv
rm -f ./test.log
unbuffer ./libcamhal_test --gtest_filter="camHalTest.mondello_*" | tee test.log | awk -F ':' '{if (match($0,/automation checkpoint/)) {system("./external_hooks.sh "$5" "$6)} else if (match($0,/PASS|FAIL|OK/)) {print $0}}'
tail test.log
rm -f /tmp/imageWidth /tmp/imageHeight /tmp/imageFormat /tmp/interlacedFlag /tmp/flagPollBuffer /tmp/flagPollStopped
