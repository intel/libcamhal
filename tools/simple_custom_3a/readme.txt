
There are two ways to build the library

(1): build the library with installing dependent rpm packages
   1. install libaiq and libcamhal rpms
   2. autoreconf --install
   3. ./configure CPPFLAGS="-I/usr/include/libiaaiq/ -I/usr/include/libcamhal/api/"
   4. make clean; make
   5. make rpm

(2): build the library with repo environment
   1. download repo code
   2. goto camera folder and source build/build-dev/env.sh
   3. mmm -j
   4. goto this library folder
   5. autoreconf --install
   6. ./configure CPPFLAGS="-I$AIQ_INSTALL_DIR/include/ia_imaging -I$LIBCAMHAL_INSTALL_DIR/include/api"
   7. make clean; make
   8. make rpm

NOTE: You need to add ${CONFIGURE_FLAGS} in "./configure" when build the library with toolchain.
eg: ./configure ${CONFIGURE_FLAGS} CPPFLAGS="-I$AIQ_INSTALL_DIR/include/ia_imaging -I$LIBCAMHAL_INSTALL_DIR/include/api"

If you want to verify your library, please install the rpm on your device.
