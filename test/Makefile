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

# if the mock test is enabled, to test in any linux host instead of target board.
#enable_mock_test = true
#BYPASS_MODE = true
#dynamic_link = true

ifndef AIQ_INSTALL_DIR
AIQ_INSTALL_DIR=/usr/
endif

ifndef IACSS_INSTALL_DIR
IACSS_INSTALL_DIR=/usr/
endif

ifndef PRE_BUILD_DIR
PRE_BUILD_DIR=/usr/
endif

ifndef CAMHAL_INSTALL_DIR
CAMHAL_INSTALL_DIR=../.libs
endif

ifdef SDKTARGETSYSROOT
sdk_root = -L$(SDKTARGETSYSROOT)/usr/lib
endif

ifneq ($(BYPASS_MODE), true)
iaaiq_headers = `pkg-config ia_imaging --cflags`
iacss_headers = `pkg-config libiacss --cflags`
ifeq ($(IPU_VER), ipu6)
libipu_headers = `pkg-config libipu6 --cflags`
libipu_libs = `pkg-config libipu6 --libs`  # contains path to ipu6 fw library
else
ifeq ($(IPU_VER), ipu4p)
libipu_headers = `pkg-config libipu4p --cflags`
libipu_libs = `pkg-config libipu4p --libs`  # contains path to ipu4p fw library
else
libipu_headers = `pkg-config libipu4 --cflags`
libipu_libs = `pkg-config libipu4 --libs`  # contains path to ipu4 fw library
endif
endif
iaaiq_libs = `pkg-config ia_imaging --libs`  # only contains paths
iacss_libs = `pkg-config libiacss --libs`  # contains libs and paths to libs

ifeq ($(ENABLE_IPU_SHARED), yes)
dynamic_link = true
ipushared_headers = `pkg-config ipushared --cflags`
ipushared_libs = `pkg-config ipushared --libs`  # only contains paths
ipushared_libs += -lia_aic -lia_aic_engine
endif
endif
gtest_headers = -I${PRE_BUILD_DIR}/include

external_headers = $(iaaiq_headers) $(iacss_headers) $(ipushared_headers) $(libipu_headers) $(gtest_headers)
internal_headers = -I../include -I../include/linux -I../include/api \
                   -I../src/v4l2 -I../src/core -I../src/3a -I../src/ -I../src/iutils \
                   -I../src/platformdata -I../src/platformdata/gc -I../src/metadata

CXXFLAGS += -g -std=c++11 -Wall -Werror $(DEFINES) -DHAVE_LINUX_OS -DHAVE_PTHREADS
CXXFLAGS += $(disabled_warning)
CXXFLAGS += $(external_headers) $(internal_headers)
CXXFLAGS += -Wno-unused-function -Wno-unused-value -fPIE -fPIC -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security
# check compiler option
CXXFLAGS += $(if $(findstring -faligned-new, $(shell $(CC) -faligned-new 2>&1)), , -faligned-new)

ifeq ($(dynamic_link),true)
dynamic_camhal = -L$(CAMHAL_INSTALL_DIR) -lcamhal
else
static_camhal = -L$(CAMHAL_INSTALL_DIR) -lcamhal
endif

static_libs = -Wl,-Bstatic -L${PRE_BUILD_DIR}/lib64/ -lgtest -lgmock -fPIC $(static_camhal)
dynamic_libs = -Wl,-Bdynamic -lpthread -lexpat $(dynamic_camhal) $(ipushared_libs) $(iaaiq_libs) $(iacss_libs) $(libipu_libs) -ldl -fPIC
disabled_warning = -Wno-sign-compare -Wno-unused-function -Wno-unused-value -Wno-unused-variable

casedir=UT
testdir=test

srcobjs = $(casedir)/case_qbuf_dqbuf.o \
	$(casedir)/case_camera_metadata.o \
	$(casedir)/case_camera_static_info.o \
	$(casedir)/case_camera_device_ops.o \
	$(casedir)/case_camera_stream_ops.o \
	$(casedir)/case_graph.o \
	$(casedir)/case_api_multithread.o \
	$(casedir)/case_camera_io_mode.o \
	$(casedir)/case_common.o \
	$(casedir)/case_dual.o \
	$(casedir)/case_virtual_channel.o

ifeq ($(enable_mock_test), true)
CXXFLAGS += -DMOCK_TEST
srcobjs += $(casedir)/MockSysCall.o
else
srcobjs += $(casedir)/case_camera_per_frame_control.o
endif

ifneq ($(BYPASS_MODE), true)
dynamic_libs += -lia_aiq -lia_mkn -lia_ob -lia_bcomp -lia_log -lia_aiqb_parser -lia_cmc_parser -lia_exc -lia_nvm -lia_coordinate -lia_ltm -lia_dvs -lia_isp_bxt -lbroxton_ia_pal -lia_lard
srcobjs+= $(casedir)/case_camera_aiq_libraries.o \
	$(casedir)/case_camera_iq_effect.o \
	$(casedir)/case_camera_cpf.o \
	$(casedir)/case_camera_parameters.o \
	$(casedir)/case_camera_3a_control.o \
	$(casedir)/case_thread.o

ifneq ($(enable_mock_test), true)
# LITE_PROCESSING_S
ifneq ($(ENABLE_IPU_SHARED), yes)
srcobjs += $(casedir)/case_camera_psyslite.o
endif
# LITE_PROCESSING_E
srcobjs += $(casedir)/case_camera_pal_p2p.o
endif

else
CXXFLAGS += -DBYPASS_MODE
endif

LDFLAGS += -L../.libs -L../build -L../ $(sdk_root) -L../.libs -L../build -L../ $(static_libs) $(dynamic_libs)
LDFLAGS += -fPIE -fPIC -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Wl,-z,relro -Wl,-z,now

mainobj=main.o

testbin=libcamhal_test

all: $(testbin)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ -c $<

$(testbin): $(mainobj) $(srcobjs)
	$(CXX) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(mainobj) $(srcobjs) $(testbin) *.yuv
