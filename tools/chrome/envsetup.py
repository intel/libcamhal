#!/usr/bin/python

#
#  Copyright (C) 2018 Intel Corporation
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

import os
import sys
import multiprocessing

IPU4_ROOT_DIR = "src/platform/intel-ipu/"
IA_IMAGING_CONTROL = "ia-imaging-control"
IA_IMAGING_ALGO_RELEASES = "algo-releases"
IPU4_HAL_DIR = "libcamhal"
# "project": "folder name"
PROJECTS = {
    "vied-viedandr-xos-ipusw": "xos-ipusw",
    "vied-viedandr-custom_config": "vied-viedandr-custom_config",
    "imaging/ia-imaging-control": "ia-imaging-control",
    "imaging/ia-imaging-tools": "ia-imaging-tools",
    "vied-viedandr-linux_drivers": "kernel",
    "vied-viedandr-libmfldadvci": "libmfldadvci",
    "vied-viedandr-cameralibs": "cameralibs",
    "vied-viedandr-libiacss": "libiacss",
    "vied-viedandr-libcamhal": "libcamhal",
    "imaging/ia_imaging-ia_ipa": "algo-releases"
}

def downloadProjects():
    for key in PROJECTS:
        cmd = "git clone ssh://icggerrit.corp.intel.com:29418/" + key + " " + PROJECTS[key]
        print cmd
        os.system(cmd)

print "start to run the envsetup.py script!===================================================="

# create ipu root dir
cwdDir = os.getcwd()
chromeRootDir = cwdDir.rstrip("env")
IPU4_ROOT_DIR = chromeRootDir + IPU4_ROOT_DIR
os.system("mkdir -p %s" % IPU4_ROOT_DIR)
os.chdir(IPU4_ROOT_DIR)
print IPU4_ROOT_DIR

# download all needed projects
downloadProjects()

# change branch for vied-viedandr-custom_config
os.chdir("%svied-viedandr-custom_config" % IPU4_ROOT_DIR)
os.system("git checkout -b bxt_1AM_feat_setup remotes/origin/bxt_1AM_feat_setup")

# change branch for algo-releases
os.chdir("%salgo-releases" % IPU4_ROOT_DIR)
os.system("git checkout -b algo_release remotes/origin/algo_release")

# build CCA and prepare to build in chrome
os.chdir(IPU4_ROOT_DIR)
ccaBuild = "CCA_BUILD"
os.system("mkdir -p %s" % ccaBuild)
os.chdir(IPU4_ROOT_DIR + ccaBuild)
print os.getcwd()

aiqCMakeEnv = "-DBUILD_SHARED_LIBS=ON \
             -DBUILD_TESTS=OFF \
             -DCMAKE_BUILD_TYPE=Release \
             -DINSTALL_BINARIES=ON \
             -DHEADERS_INSTALL_DIRECTORY=include/ia_imaging \
             -DBINARIES_INSTALL_DIRECTORY=lib \
             -DCMAKE_INSTALL_PREFIX=./INSTALL \
             -DIPU_VER= \
             -DCMAKE_C_FLAGS='-w' \
             -DCMAKE_CXX_FLAGS='-w' \
             "
os.environ["AIQ_CMAKE"] = aiqCMakeEnv
os.system("cmake $AIQ_CMAKE ../%s" % IA_IMAGING_CONTROL)
threadNum = multiprocessing.cpu_count() - 1
os.system("make -j%s" % threadNum)

os.system("rm CMakeCache.txt CMakeFiles/ onesource/CMakeFiles/ -rf")
os.chdir(IPU4_ROOT_DIR + IA_IMAGING_CONTROL)
os.system("sed -i '/mono not found/d' CMakeLists.txt")

os.chdir(IPU4_ROOT_DIR + IA_IMAGING_ALGO_RELEASES)
os.system("sed -i '7,11d' onesource/osxml/CMakeLists.txt")

os.chdir(IPU4_ROOT_DIR + IPU4_HAL_DIR)
ret = os.system("git log | grep I7448854321ed3595dc3edb1c0ee15e3cd50d17c4")
if ret != 0:
    ret = os.system("git am tools/chrome/0001-libcamhal-chrome-workaround-to-build.patch")
    if ret != 0:
        print "applying patch failed! please check if there're conflicts!"
else:
    print "patch 0001-libcamhal-chrome-workaround-to-build.patch already applied! exiting script!"

print "Finish running the envsetup.py script!===================================================="

