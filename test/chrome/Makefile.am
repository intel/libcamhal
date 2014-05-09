# Copyright (C) 2017-2018 Intel Corporation
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

AUTOMAKE_OPTIONS = foreign
ACLOCAL_AMFLAGS = -I m4

# Unit test app
bin_PROGRAMS = camera_subsys_test
#bin_SCRIPTS = camera_subsys_test_linux.sh

camera_subsys_test_SOURCES = src/main.cpp \
                         src/hal_module_basic_tests.cpp \
                         src/raw_hal_tests.cpp \
                         src/raw_hal_test_yuv_resolution.cpp \
                         src/raw_hal_test_jpeg_resolution.cpp \
                         src/supported_streams_test.cpp \
                         src/test_parameterization.cpp \
                         src/test_stream_factory.cpp \
                         src/valgrind_test.cpp \
                         src/multi_streams_test.cpp \
                         src/per_frame_test.cpp \
                         src/control_3a_test.cpp \
                         src/pnp_aal_test.cpp

if GBM_BUFFER
camera_subsys_test_SOURCES += \
                         src/external/camera3_test_gralloc.cc
endif

camera_subsys_test_CPPFLAGS = -DNDEBUG -std=c++11 \
                            -I./src/external \
			    $(ARC_CAMERA3_ANDROID_HEADERS_CFLAGS) \
                            $(LIBCAMERA_CLIENT_CFLAGS) \
                            $(LIBCAMERA_METADATA_CFLAGS) \
                            $(LIBCBM_CFLAGS) \
                            $(LIBDRM_CFLAGS) \
                            $(LIBCHROME_CFLAGS) \
                            $(LIBSYNC_CFLAGS)

#includes

if GBM_BUFFER
camera_subsys_test_CPPFLAGS += \
                          -DGBM_BUFFER
endif

camera_subsys_test_LDADD = -lgtest \
                       -lpthread


# libs
EXTERNAL_LIBS = -ljpeg -lexpat -lpthread $(LIBDRM_LIBS) $(LIBCBM_LIBS) $(LIBCHROME_LIBS)
UTIL_LIBS = $(PSLLIBS) \
            $(IA_IMAGING_LIBS) \
            $(LIBIACSS_LIBS)
STATIC_LIBS = $(LIBCAMERA_CLIENT_LIBS) \
              $(LIBCAMERA_METADATA_LIBS) \
              $(LIBSYNC_LIBS)
OTHER_LIBS = -ldl
camera_subsys_test_LDADD += $(EXTERNAL_LIBS) $(UTIL_LIBS) $(CAMERALIBS_LIBS) $(STATIC_LIBS) $(OTHER_LIBS)

if GBM_BUFFER
camera_subsys_test_LDADD += \
                       -lgbm \
                       -ldrm \
                       -lcbm
endif

# install suppression file
suppressiondir = /usr/share/camera_subsys_test
suppression_DATA = unittest.supp
