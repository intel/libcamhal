
Copyright (C) 2016-2017 Intel Corporation

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

     http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.


# Table of Contents
1. [Introduction](##Introduction)
2. [libcamhal Build Guide](##libcamhal-Build-Guide)
3. [UT Build Guide](##UT-Build-Guide)
4. [Debug](##Debug)

## Introduction
### What is libcamhal
libcamhal is the implement of "Linux Camera HAL".
It is the Hardware Abstract Layer for IPUx. It is the integration points for all
the user space libraries.

> Note:
> The libcamhal could support all the Linux famality products include Android and Chrome.

The Linux Camera HAL is a user space camera drivers to provide the abstract camera
functions to upper layer frameworks and applications.
Linux Camera HAL is a components that interprets requests from applications
(gstreamer camera source, Android/Chrome camera service) and converts the requests
to IPU drivers and other image components.
The HAL glues all the camera components together and its responsibility are
 * Provide the Camera HAL API to applications
 * Configure IPU ISYS drivers with Media Controller API
 * Configure IPU PSYS drivers with libiacss (CIPF/CIPR)
 * Get frames from the IPU drivers
 * Relaying and converting user parameters to 3A/IPU parameters
 * Control of the camera sensors and motor
 * Run the 3A Loop with AIQ library. Get statistics from IPU and set the 3A results to the camera sensors and IPU driver
 * Synchronizing statistics and parameters associated with the statistics (queue)


Please refer to "Broxton Linux Camera HAL High Level Design" in the SharePoint
	for more information about the design.
Please refer to wiki of "libcamhal developer guide" about
 * How to submit code
 * How to sync code
 * How to build with other components
 * Coding Style

### Folder Introduction

| Folder    | sub folder    | Description  |
| ----------|:-------------:| -----:|
| include/  | api | The libcamhal exported API |
|           | utils         |   The libcamhal exported API for debug usage |
|           | linux         |   The copied ISP driver header files |
| halv1     | | The Android Camera HAL Version 1.x implement based on libcamhal API |
| halv3     | | The Android Camera HAL Version 3.x implement based on libcamhal API |
| src       | | The source code directory |
| tools     | | Miscellaneous tools for developers |
| test      | | The Unit Test Cases for libcamhal |


## libcamhal Build Guide
    1. Setup
    2. Building libcamhal with Cmake

### Setup
    Prerequisites (Ubuntu 14.04/16.04):
    sudo apt-get install autoconf libtool linux-libc-dev
    install the gmock and gtest if you need build the test

### Build libcamhal
    mkdir build
    cd build
    cmake ../
    make install -j
    cpack

    Here are some settings for building libcamhal:
    1. To enable the graph config parser for ipu6, please set "-DIPU_VER=ipu6" when run cmake:
       cmake -DIPU_VER=ipu6 ../
    2. To enable the ATE, please set "-DENABLE_VIRTUAL_IPU_PIPE=true" when run cmake:
       cmake -DENABLE_VIRTUAL_IPU_PIPE=true ../
    3. To enable the BYPASS MODE, please set "-DBYPASS_MODE=true" when run cmake:
       cmake -DBYPASS_MODE=true ../
    4. To install the libraries/headers/configure files to the specified path, please set
       "-DCMAKE_INSTALL_PREFIX=" when run cmake, like:
       cmake -DCMAKE_INSTALL_PREFIX=$LIBCAMHAL_INSTALL_DIR ../
    5. Libcamhal depends on libiaaiq, libipu and libiacss. By default, it searches these
       packages from /usr/lib/pkgconfig. To specify the pkgconfig path, set PKG_CONFIG_PATH
       before run cmake, like:
       export PKG_CONFIG_PATH=$PKG_CONFIG_PATH:$AIQ_INSTALL_DIR/lib/pkgconfig
       cmake ../
    6. To specify the rpm package version, release and package file name, please set the CPACK
       variables when run cpack, like:
       cpack -D CPACK_RPM_PACKAGE_VERSION="1.0.0" \
             -D CPACK_RPM_PACKAGE_RELEASE="YYYYMMDD" \
             -D CPACK_PACKAGE_FILE_NAME="libcamhal-1.0.0-YYYYMMDD.x86_64"

## UT Build Guide
### Build Test
    cd test
    make -j

### Run Test
    ./libcamhal_test

## Debug
### Print Debug Log
    To print the debug log, you can use the environment variable "cameraDebug".
    export cameraDebug=xxx
    Please check the CameraLog.h for detailed supported values.
### Dump Frames
    Linux camera HAL dump function is controlled by an environment named: "cameraDump".
    The env would be checked once when camera HAL module being loaded.
    So to enable the dump functionality you need to set the env before starting your test program.
    export cameraDump=xxx
    Please refer to CameraDump.h for detailed supported values.
### File Injection
    In File injection mode, you can specify a frame file as the input of PSYS,
    and it can help debug PSYS related functions easily.

    There are two working modes:
    1. The simple mode which only provides one same frame for all sequences.
       Enabled by: export cameraInjectFile="FrameFileName"
    2. The advanced mode which can configure which file is used for any sequence or FPS.
       Enabled by: export cameraInjectFile="ConfigFileName.xml"
       You can find a sample config file in config/file_src_config_sample.xml of libcamhal repo.
> Note: As you can see both simple mode and advanced mode are using one same environment variable
> "**cameraInjectFile**", if cameraInjectFile is set to a configure file which ends with ".xml",
> then advanced mode is activated, and FileSoure will read the configure file to produce
> the frame buffers. If cameraInjectFile is set to an file name which doesn't not end with
> ".xml", then it will be treated as a frame file which will be read directly as frame buffer.
