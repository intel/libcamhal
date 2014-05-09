#!/bin/bash
#
#  Copyright (C) 2016 Intel Corporation
#
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

usage() {
    echo "Usage: -s/--scan [-f/--fix] dir_path"
    echo "    dir_path:  absolute or relative path of directory to check"
    echo "    -s|--scan  scan copyright in files"
    echo "    -f|--fix   fix the copyright in files"
}


############
# Main
############

distro=`cat /etc/issue | grep "Ubuntu 14.04"`

if [ ! -n "$distro" ];then
    echo "Warning: the script only works on Ubuntu distro, above 14.04 version."
    exit 1
fi

for i in "$*"
do
    args=$i
done

set -- $(getopt hs:f: "$@")
while [ $# -gt 0 ]
do
    case "$1" in
        (-h|--help) usage; exit 0;;
        (-s|--scan) shift ;;
        (-f|--fix) shift ;;
        (--) shift; break;;
        (-*) echo "$0: error - unrecognized option $1" 1>&2; exit 1;;
        (*)  break;;
        esac
        shift
done


python check_update_copyright.py $args
