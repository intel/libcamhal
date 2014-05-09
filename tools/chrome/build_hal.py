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
import subprocess

IPU4_ROOT_DIR = "platform/intel-ipu/"
CAMERA_HAL = "libcamhal"
IA_IMAGING_CONTROL = "ia-imaging-control"
AIQ = "libmfldadvci"
AIQ_OUT_DIR = ""
XOS_IPUSW = "xos-ipusw"
XOS_IPUSW_OUT_DIR = "out/" + XOS_IPUSW + "/"
AIQB = "cameralibs"
AIQB_OUT_DIR = "out/" + AIQB + "/"
IACSS = "libiacss"
IACSS_OUT_DIR = "out/" + IACSS + "/"
IPU4P_CFG_EBUILD = "private-overlays/overlay-cnlrvp-private/media-libs/cros-camera-hal-configs-cnlrvp/"
IPU4P_LIBS_EBUILD = "private-overlays/overlay-cnlrvp-private/media-libs/intel-3a-libs-bin-cnlrvp/"
SRC_ROOT_DIR = ""

print "start to run the build_hal.py script!===================================================="
cwdDir = os.getcwd()
SRC_ROOT_DIR = cwdDir.rstrip("scripts")
print SRC_ROOT_DIR
IPU4_ROOT_DIR = SRC_ROOT_DIR + IPU4_ROOT_DIR
print IPU4_ROOT_DIR

# build cca
os.chdir(IPU4_ROOT_DIR + "CCA_BUILD")
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
os.system("make install")

# copy the cca libs into the libmfldadvci
os.system("cp -r INSTALL/include/ia_imaging/* ../%s/ia_imaging/include/" % AIQ)
os.system("cp -r INSTALL/lib/* ../%s/ia_imaging/linux/lib/release/64/" % AIQ)

# build libmfldadvci
os.chdir(IPU4_ROOT_DIR + AIQ)
AIQ_OUT_DIR = IPU4_ROOT_DIR + "out/" + AIQ + "/"
print AIQ_OUT_DIR
os.system("autoreconf --install")
os.system("./configure --with-project=dss --prefix=%s" % AIQ_OUT_DIR)
os.system("make install")

# build cameralibs
os.chdir(IPU4_ROOT_DIR + AIQB)
AIQB_OUT_DIR = IPU4_ROOT_DIR + "out/" + AIQB + "/"
print AIQB
print AIQB_OUT_DIR
os.environ["PKG_CONFIG_PATH"] = AIQ_OUT_DIR + "/lib/pkgconfig"
os.system("autoreconf --install")
os.system("./configure --prefix=%s" % AIQB_OUT_DIR)
os.system("make -j")
os.system("make install")

# build ipu4p
os.chdir(IPU4_ROOT_DIR + XOS_IPUSW)
XOS_IPUSW_OUT_DIR = IPU4_ROOT_DIR + "out/" + XOS_IPUSW + "/"
print XOS_IPUSW_OUT_DIR
os.system("autoreconf --install")
os.system("./configure --prefix=%s" % XOS_IPUSW_OUT_DIR)
os.system("make -j")
os.system("make install")

# copy the ipu4p content to cameralibs
os.system("cp %slib/libipu4p.* %slib/." % (XOS_IPUSW_OUT_DIR, AIQB_OUT_DIR))
os.system("rm %sinclude/libipu/* -rf" % AIQB_OUT_DIR)
os.system("cp %sinclude/libipu4p/* %sinclude/libipu/." % (XOS_IPUSW_OUT_DIR, AIQB_OUT_DIR))
libipu4pPCDir = SRC_ROOT_DIR + IPU4P_LIBS_EBUILD + "files/usr/lib64/pkgconfig/"
os.system("cp %slibipu4p.pc %slib/pkgconfig" % (libipu4pPCDir, AIQB_OUT_DIR))

os.chdir(AIQB_OUT_DIR + "lib/pkgconfig/")
ipu4pPC = AIQB_OUT_DIR + "lib/pkgconfig/libipu4p.pc"
with open(ipu4pPC, 'r') as inFile, open('tmp', 'w') as outFile:
    for line in inFile:
        if "/usr" in line:
            outFile.write("prefix=%s\n" % AIQB_OUT_DIR)
        elif "lib64" in line:
            outFile.write("libdir=${exec_prefix}/lib\n")
        else:
            outFile.write(line)
os.remove(ipu4pPC)
os.system("mv tmp %s" % ipu4pPC)

# build libiacss
libiacssDir = IPU4_ROOT_DIR + IACSS
os.chdir(libiacssDir)
IACSS_OUT_DIR = IPU4_ROOT_DIR + "out/" + IACSS + "/"
print IACSS_OUT_DIR
os.environ["PKG_CONFIG_PATH"] = AIQ_OUT_DIR + "/lib/pkgconfig:" + AIQB_OUT_DIR + "/lib/pkgconfig"
os.system("autoreconf --install")
os.system("./configure --with-kernel-sources=%s/kernel/ --with-B0=yes --with-aiq=yes --with-ipu=ipu4p --prefix=%s" % (IPU4_ROOT_DIR, IACSS_OUT_DIR))
os.system("make -j")
os.system("make install")
# clean the libiacss source code folder
os.system("rm %s/* -rf" % libiacssDir)
os.system("git checkout *")

# copy the libs and header files into ebuild
libsEbuildDir = SRC_ROOT_DIR + IPU4P_LIBS_EBUILD + "files/usr/lib64/"
headersEbuildDir = SRC_ROOT_DIR + IPU4P_LIBS_EBUILD + "files/usr/include/"
# libipu4p
os.system("cp %slib/libipu4p.so* %s" % (AIQB_OUT_DIR, libsEbuildDir))
os.system("cp %sinclude/* %s/. -rf" % (AIQB_OUT_DIR, headersEbuildDir))
# aiq
os.system("cp %slib/*.so %s" % (AIQ_OUT_DIR, libsEbuildDir))
os.system("cp %slib/*.a %s" % (AIQ_OUT_DIR, libsEbuildDir))
os.system("cp %sinclude/* %s/. -rf" % (AIQ_OUT_DIR, headersEbuildDir))
#iacss
os.system("cp %sinclude/* %s/. -rf" % (IACSS_OUT_DIR, headersEbuildDir))

#copy cfg xmls in libcamhal to cros-camera-hal-configs-cnlrvp
libcamhalCfgDir = IPU4_ROOT_DIR + CAMERA_HAL + "/config/ipu4/"
print libcamhalCfgDir
CFG_FILES = {#"libcamhal_profile.xml": "files/",
             "psys_policy_profiles.xml": "files/",
             "gcss/graph_descriptor.xml": "files/gcss/",
             "gcss/graph_settings_imx319.xml": "files/gcss/",
             "gcss/graph_settings_imx355.xml": "files/gcss/",
             "sensors/imx319.xml": "files/sensors/",
             "sensors/imx355.xml": "files/sensors/"}
os.chdir(libcamhalCfgDir)
for f in CFG_FILES:
    target = SRC_ROOT_DIR + IPU4P_CFG_EBUILD + CFG_FILES[f]
    os.system("cp %s %s -rf" % (f, target))

#remove unused libs and headers
# todo
print "Finish running the build_hal.py script!===================================================="

