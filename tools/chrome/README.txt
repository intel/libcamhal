Please following the wiki to do:
https://wiki.ith.intel.com/display/ICGglob/Setup+chrome+camera+hal+compiling+env+for+cnl

Notes:
1. the envsetup.py must be put to the chromeroot/env/.
   and run it after the chrome code is downloaded before running the cros_sdk.
2. the build_hal.py must be put to the chromeroot/src/scripts/.
   and run it before the build_packages after running the cros_sdk.
3. 0001-libcamhal-chrome-workaround-to-build.patch is a workaround solution
   to enable cnlrvp build environment, its content will soon be merged into repo
