camera_subsys_test
==================

Introduction
------------

The camera_subsys_test is an unit test tool for camera HAL, run on DUT.

Usage:

  camera_subsys_test --help
  camera_subsys_test --gtest_list_tests

Example:

  camera_subsys_test --gtest_filter=yuv_resolutions/RawHal_Test_Yuv_Resolutions.TestYuvNo3a/0

Compile
-------

The UT tool will be built with:

  1. cd google/cnlrvp
  2. ./chromite/bin/cros_sdk
  3. cd ~/trunk/src/private-overlays/overlay-cnlrvp-private/media-libs/cros-camera-hal-cnlrvp/files/test/chrome
  4. autoreconf --install
  5. export BOARD=cnlrvp
  6. run:
    PKG_CONFIG_SYSROOT_DIR=/build/cnlrvp \
    PKG_CONFIG_PATH=/build/${BOARD}/usr/lib64/pkgconfig/ \
     ./configure --prefix=/usr --with-base-version=395517 \
       CPPFLAGS="-I/build/${BOARD}/usr/include -I${HOME}/trunk/src/platform/arc-camera/include -I${HOME}/trunk/src/platform/arc-camera -I${HOME}/trunk/src/platform/arc-camera/android/libcamera_metadata/include/ -I${HOME}/trunk/src/platform/arc-camera/android/libcamera_client/include/ -I${HOME}/trunk/src/platform/arc-camera/android/header_files/include/system/core/include/ -I${HOME}/trunk/src/platform/arc-camera/android/header_files/include/hardware/libhardware/include/ -I${HOME}/trunk/src/platform/minigbm -I${HOME}/trunk/src/private-overlays/overlay-cnlrvp-private/media-libs/cros-camera-hal-cnlrvp/files/src" \
       LDFLAGS="-L/build/${BOARD}/usr/lib64" \
       --with-gbmbuf \
       --with-sysroot=/build/${BOARD}
  6. make -j2

The compiled UT tool binary located at current directory:

  camera_subsys_test

