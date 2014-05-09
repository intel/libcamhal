/*
 * Copyright (C) 2016-2018 Intel Corporation.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#define LOG_TAG "CASE_CIPR"

#include "psyslite/WeavingPipeline.h"
#include "psyslite/CscPipeline.h"
#include "psyslite/ScalePipeline.h"
#include "psyslite/FisheyePipeline.h"

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "linux/videodev2.h"
#include "iutils/Utils.h"
#include "iutils/CameraDump.h"
#include "iutils/CameraLog.h"
#include "ICamera.h"
#include "case_common.h"
#include "PlatformData.h"

#include "MockSysCall.h"
#include "gtest/gtest.h"

#define PAGE_SIZE (getpagesize())

static std::shared_ptr<CameraBuffer> util_get_file(const char *file, size_t *len)
{
    long file_size;
    FILE *f;
    int r;

    if (!file || !len)
        return nullptr;

    f = fopen(file, "rb");
    if (!f) {
        LOGD("failed to open input file: %s", file);
        return nullptr;
    }

    fseek(f, 0, SEEK_END);
    file_size = ftell(f);
    rewind(f);

    std::shared_ptr<CameraBuffer> buf =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (file_size + PAGE_SIZE), 0);
    if (!buf) {
        return nullptr;
    }

    r = fread(buf->getBufferAddr(), file_size, 1, f);
    if (r != 1) {
        fclose(f);
        LOGD("%s, failed to read input file\n", __func__);
        return nullptr;
    }

    r = fclose(f);
    if (r) {
        LOGD("%s, closing file %s failed\n", __func__, file);
        return nullptr;
    }

    *len = file_size;

    return buf;
}

static int util_write_to_file(const char* name, uint8_t *data, size_t bytes)
{
    if (!CameraDump::isDumpTypeEnable(DUMP_UT_BUFFER)) return 0;

    FILE *f;
    int r;

    f = fopen(name, "wb");
    if (!f) {
        LOGD("can't open file %s", name);
        return -errno;
    }

    r = fwrite(data, bytes, 1, f);
    if (r != 1) {
        fclose(f);
        LOGD("failed to write to file");
        return -errno;
    }

    r = fclose(f);
    if (r) {
        LOGD("failed to close file");
        return -errno;
    }

    return 0;
}

class TestBasePipe : public PSysPipeBase {
public:
    TestBasePipe() : PSysPipeBase(-1) {}
    bool isPgIdSupported(int pgId) { return (getManifest(pgId) == OK); }
    int prepareTerminalBuffers(vector<std::shared_ptr<CameraBuffer>>& srcBufs,
                                    vector<std::shared_ptr<CameraBuffer>>& dstBufs) {
        return 0;
    }
};

static bool isPgIdSupported(int pgId)
{
    TestBasePipe pipeline;
    return pipeline.isPgIdSupported(pgId);
}

static int createDMABuffer(int context, int size, int* fd, void** buf)
{
    struct intel_ipu4_psys_buffer {
        uint64_t len;
        void* userptr;
        int fd;
        uint32_t flags;
        uint32_t reserved[2];
    } __attribute__((packed));
    #define INTEL_IPU4_IOC_GETBUF _IOWR('A', 4, struct intel_ipu4_psys_buffer)

    struct intel_ipu4_psys_buffer psysBuf;
    CLEAR(psysBuf);
    psysBuf.len = size;
    psysBuf.userptr = IA_CIPR_ALLOC_ALIGNED(PAGE_ALIGN(size), IA_CIPR_PAGESIZE());
    int res = ioctl(context, (int)INTEL_IPU4_IOC_GETBUF, (void*)&psysBuf);
    Check((res < 0), -1, "@%s, line:%d, call ioctl for INTEL_IPU4_IOC_GETBUF fail, res:%d", __func__, __LINE__, res);

    *fd = psysBuf.fd;
    *buf = psysBuf.userptr;
     return 0;
}

static int destroyDMABuffer(int fd, void* buf)
{
    int res = close(fd);
    int error = errno;

    Check((res < 0), -1, "@%s, line:%d, call ioctl for close fail, res:%d, close returned error:%s",
        __func__, __LINE__, res, strerror(error));

    IA_CIPR_FREE(buf);

    return 0;
}

class TestWeavingPipeline : public WeavingPipeline {
public:
    TestWeavingPipeline() : WeavingPipeline() {}
    ~TestWeavingPipeline() {}

public:
   int createDMABuffer(int size, int* fd, void** buf)
   {
       return ::createDMABuffer((int)(*((int*)mCtx)), size, fd, buf);
   }
};

class TestCscPipeline : public CscPipeline {
public:
    TestCscPipeline() : CscPipeline() {}
    ~TestCscPipeline() {}

public:
    int createDMABuffer(int size, int* fd, void** buf)
    {
        return ::createDMABuffer((int)(*((int*)mCtx)), size, fd, buf);
    }
};

class TestScalePipeline : public ScalePipeline {
public:
    TestScalePipeline() : ScalePipeline() {}
    ~TestScalePipeline() {}

public:
    int createDMABuffer(int size, int* fd, void** buf)
    {
        return ::createDMABuffer((int)(*((int*)mCtx)), size, fd, buf);
    }
};

int verify_pixel_data(int fmt, int w, int h, uint8_t* top, uint8_t* bottom, uint8_t* dst)
{
    int srcWidth = w;
    int srcHeight = h;
    int dstWidth = w;
    int dstHeight = h * 2;
    int bpl = CameraUtils::getStride(fmt, w);

    for (int row = 0; row < srcHeight; row++) {
        for (int col = 0; col < bpl; col++) {
            if (dst[((row * 2) * bpl ) + col] != top[row * bpl + col]) {
                return -1;
            }
            if (dst[((row * 2 + 1) * bpl) + col] != bottom[row * bpl + col]) {
                return -2;
            }
        }
    }

    return 0;
}

void set_port_frame_info(FrameInfoPortMap& Frame, Port port, int w, int h, int fmt)
{
    FrameInfo frameInfo;

    frameInfo.mWidth = w;
    frameInfo.mHeight = h;
    frameInfo.mFormat = fmt;
    frameInfo.mBpp = CameraUtils::getBpp(frameInfo.mFormat);
    frameInfo.mStride = CameraUtils::getStride(frameInfo.mFormat, frameInfo.mWidth);
    Frame[port] = frameInfo;
}

void test_with_frame_files(int fmt, int w, int h, const char* top, const char* bottom, const char* dst)
{
    if (!isPgIdSupported(WeavingPipeline::PG_ID)) return;

    size_t s;
    std::shared_ptr<CameraBuffer> pTop = util_get_file(top, &s);
    if (!pTop) {
        LOGD("@%s, fail to get the file for top frame, skip testing...", __func__);
        return;
    }

    std::shared_ptr<CameraBuffer> pBottom = util_get_file(bottom, &s);
    if (!pBottom) {
        LOGD("@%s, fail to get the file for bottom frame, skip testing...", __func__);
        return;
    }

    int srcWidth = w;
    int srcHeight = h;
    int dstWidth = w;
    int dstHeight = h * 2;
    int outSize = CameraUtils::getFrameSize(fmt, dstWidth, dstHeight);

    std::shared_ptr<CameraBuffer> pDst =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (outSize + PAGE_SIZE), 0);
    EXPECT_NE(pDst, nullptr);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcWidth, srcHeight, fmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstWidth, dstHeight, fmt);

    PSysPipeBase* pipeline = new WeavingPipeline();
    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    int ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in = {pTop, pBottom};
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);
    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    util_write_to_file(dst, static_cast<uint8_t*>(pDst->getBufferAddr()), outSize);

    ret = verify_pixel_data(fmt, w, h,
                            static_cast<uint8_t*>(pTop->getBufferAddr()),
                            static_cast<uint8_t*>(pBottom->getBufferAddr()),
                            static_cast<uint8_t*>(pDst->getBufferAddr()));
    EXPECT_EQ(ret, 0) << "Fixel data incorrect." << "(" << w << "x" << h << ")";

    delete pipeline;
}

void test_with_autogen_frames(int fmt, int w, int h)
{
    if (!isPgIdSupported(WeavingPipeline::PG_ID)) return;

    int srcWidth = w;
    int srcHeight = h;
    int dstWidth = w;
    int dstHeight = h * 2;
    int inSize = CameraUtils::getFrameSize(fmt, srcWidth, srcHeight);
    int outSize = CameraUtils::getFrameSize(fmt, dstWidth, dstHeight);

    std::shared_ptr<CameraBuffer> pTop =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (inSize + PAGE_SIZE), 0);
    EXPECT_NE(pTop, nullptr);

    std::shared_ptr<CameraBuffer> pBottom =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (inSize + PAGE_SIZE), 0);
    EXPECT_NE(pBottom, nullptr);

    std::shared_ptr<CameraBuffer> pDst =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (outSize + PAGE_SIZE), 0);
    EXPECT_NE(pDst, nullptr);

    memset(pTop->getBufferAddr(), 0x80, inSize);
    memset(pBottom->getBufferAddr(), 0x40, inSize);
    memset(pDst->getBufferAddr(), 0xFF, outSize);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcWidth, srcHeight, fmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstWidth, dstHeight, fmt);

    PSysPipeBase* pipeline = new WeavingPipeline();

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    int ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in = {pTop, pBottom};
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    char fileName[100] = { '\0' };
    const char* formatName = (fmt == V4L2_PIX_FMT_SGRBG8) ? "BIN8" : CameraUtils::format2string(fmt);

    static int count = 0;
    count++;
    snprintf(fileName, sizeof(fileName), "cam_frame_%03d_%dx%d_autogen_output.%s",
             count, dstWidth, dstHeight, formatName);
    util_write_to_file(fileName, static_cast<uint8_t*>(pDst->getBufferAddr()), outSize);

    ret = verify_pixel_data(fmt, w, h,
                            static_cast<uint8_t*>(pTop->getBufferAddr()),
                            static_cast<uint8_t*>(pBottom->getBufferAddr()),
                            static_cast<uint8_t*>(pDst->getBufferAddr()));
    EXPECT_EQ(ret, 0) << "Fixel data incorrect." << "(" << w << "x" << h << ")";

    delete pipeline;
}

TEST(camCiprTest, printManifest)
{
    TestBasePipe pipeline();
}

TEST(camCiprTest, weaving_pg_all_binary8)
{
    // This will test monochromatic images with width from 256 to 2560
    for (int i = 1; i <= 10; i++) {
        // Use V4L2_PIX_FMT_SGRBG8 for binary8 format since HAL don't have the same format as
        // CSS does. Maybe after the weaving pg fully verified, we will remove the cases for binary8.
        test_with_autogen_frames(V4L2_PIX_FMT_SGRBG8, 256 * i, 256);
    }
}

TEST(camCiprTest, weaving_pg_uyvy_all_autogen)
{
    // This will test UYVY images with width from 256 to 2560
    for (int i = 1; i <= 10; i++) {
        test_with_autogen_frames(V4L2_PIX_FMT_UYVY, 256 * i, 256);
    }
}

TEST(camCiprTest, weaving_pg_uyvy_720x480_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_UYVY, 720, 240);
}

TEST(camCiprTest, weaving_pg_uyvy_720x576_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_UYVY, 720, 288);
}

TEST(camCiprTest, weaving_pg_uyvy_1920x1080_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_UYVY, 1920, 540);
}

TEST(camCiprTest, weaving_pg_uyvy_1080i)
{
    test_with_frame_files(V4L2_PIX_FMT_UYVY, 1920, 540,
            "cam_frame_1920x540_top.UYVY",
            "cam_frame_1920x540_bottom.UYVY",
            "cam_frame_1920x1080_file_output.UYVY");
}

TEST(camCiprTest, weaving_pg_rgb565_720x480_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_RGB565, 720, 240);
}

TEST(camCiprTest, weaving_pg_rgb565_1920x1080_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_RGB565, 1920, 540);
}

TEST(camCiprTest, weaving_pg_rgb888_720x480_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_RGB24, 720, 240);
}

TEST(camCiprTest, weaving_pg_rgb888_1920x1080_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_RGB24, 1920, 540);
}

TEST(camCiprTest, weaving_pg_argb_720x480_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_RGB32, 720, 240);
}

TEST(camCiprTest, weaving_pg_argb_1920x1080_autogen)
{
    return; // Too wide to support.
    test_with_autogen_frames(V4L2_PIX_FMT_RGB32, 1920, 540);
}

TEST(camCiprTest, weaving_pg_nv16_720x480_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_NV16, 720, 240);
}

TEST(camCiprTest, weaving_pg_nv16_1920x1080_autogen)
{
    test_with_autogen_frames(V4L2_PIX_FMT_NV16, 1920, 540);
}

TEST(camCiprTest, weaving_pg_binary8_512x512)
{
    test_with_frame_files(V4L2_PIX_FMT_SGRBG8, 512, 256,
            "cam_frame_512x256_top.BIN8",
            "cam_frame_512x256_bottom.BIN8",
            "cam_frame_512x512_file_output.BIN8");
}

TEST(camCiprTest, weaving_pg_binary8_720x512)
{
    test_with_frame_files(V4L2_PIX_FMT_SGRBG8, 720, 256,
            "cam_frame_720x256_top.BIN8",
            "cam_frame_720x256_bottom.BIN8",
            "cam_frame_720x512_file_output.BIN8");
}

TEST(camCiprTest, weaving_pg_binary8_1024x512)
{
    test_with_frame_files(V4L2_PIX_FMT_SGRBG8, 1024, 256,
            "cam_frame_1024x256_top.BIN8",
            "cam_frame_1024x256_bottom.BIN8",
            "cam_frame_1024x512_file_output.BIN8");
}

TEST(camCiprTest, weaving_pg_binary8_1440x512)
{
    test_with_frame_files(V4L2_PIX_FMT_SGRBG8, 1440, 256,
            "cam_frame_1440x256_top.BIN8",
            "cam_frame_1440x256_bottom.BIN8",
            "cam_frame_1440x512_file_output.BIN8");
}

TEST(camCiprTest, weaving_pg_binary8_512x512_buffer_in_dma_out)
{
    if (!isPgIdSupported(WeavingPipeline::PG_ID)) return;

    LOGD("@%s, line:%d camCiprTest.weavingPG begin!", __func__, __LINE__);
    size_t s;
    std::shared_ptr<CameraBuffer> pTop = util_get_file("cam_frame_512x256_top.BIN8", &s);
    if (!pTop) {
        LOGD("@%s, fail to get the file for top frame, skip testing...", __func__);
        return;
    }

    std::shared_ptr<CameraBuffer> pBottom = util_get_file("cam_frame_512x256_bottom.BIN8", &s);
    if (!pBottom) {
        LOGD("@%s, fail to get the file for bottom frame, skip testing...", __func__);
        return;
    }

    TestWeavingPipeline* pipeline = new TestWeavingPipeline();

    int srcWidth = 512;
    int srcHeight = 256;
    int dstWidth = srcWidth;
    int dstHeight = srcHeight * 2;
    int fmt = V4L2_PIX_FMT_SGRBG8;

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcWidth, srcHeight, fmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstWidth, dstHeight, fmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    int dstFd = 0;
    void* dstBuf = nullptr;
    int ret = pipeline->createDMABuffer(dstWidth * dstHeight, &dstFd, &dstBuf);
    EXPECT_EQ(ret, 0);

    std::shared_ptr<CameraBuffer> pDst =
        std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, dstWidth * dstHeight, 0);
    EXPECT_NE(pDst, nullptr);

    icamera::camera_buffer_t bufInfo;
    bufInfo.s.memType = V4L2_MEMORY_DMABUF;
    bufInfo.addr = dstBuf;
    bufInfo.dmafd = dstFd;
    pDst->setUserBufferInfo(&bufInfo);

    ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    int inSize = srcWidth * srcHeight;
    int outSize = dstWidth * dstHeight;

    vector<std::shared_ptr<CameraBuffer>> in ={pTop, pBottom};
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    util_write_to_file("cam_frame_512x512_dma_file_output.BIN8", static_cast<uint8_t*>(dstBuf), outSize);

    ret = verify_pixel_data(fmt, srcWidth, srcHeight,
                            static_cast<uint8_t*>(pTop->getBufferAddr()),
                            static_cast<uint8_t*>(pBottom->getBufferAddr()),
                            static_cast<uint8_t*>(dstBuf));
    EXPECT_EQ(ret, 0) << "Fixel data incorrect." << "(" << srcWidth << "x" << srcHeight << ")";

    ret = destroyDMABuffer(dstFd, dstBuf);
    EXPECT_EQ(ret, 0);

    delete pipeline;
}

TEST(camCiprTest, weaving_pg_binary8_512x512_dma_in_dma_out)
{
    if (!isPgIdSupported(WeavingPipeline::PG_ID)) return;

    LOGD("@%s, line:%d camCiprTest.weavingPG begin!", __func__, __LINE__);
    size_t s;
    std::shared_ptr<CameraBuffer> top = util_get_file("cam_frame_512x256_top.BIN8", &s);
    if (!top) {
        LOGD("@%s, fail to get the file for top frame, skip testing...", __func__);
        return;
    }

    std::shared_ptr<CameraBuffer> bottom = util_get_file("cam_frame_512x256_bottom.BIN8", &s);
    if (!bottom) {
        LOGD("@%s, fail to get the file for bottom frame, skip testing...", __func__);
        return;
    }

    TestWeavingPipeline* pipeline = new TestWeavingPipeline();

    int srcWidth = 512;
    int srcHeight = 256;
    int dstWidth = srcWidth;
    int dstHeight = srcHeight * 2;
    int fmt = V4L2_PIX_FMT_SGRBG8;

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcWidth, srcHeight, fmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstWidth, dstHeight, fmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    int inSize = srcWidth * srcHeight;
    int outSize = dstWidth * dstHeight;

    int inFd1 = -1;
    void* inBuf1 = nullptr;
    int ret = pipeline->createDMABuffer(inSize, &inFd1, &inBuf1);
    ASSERT_EQ(ret, 0);
    memcpy(inBuf1, top->getBufferAddr(), inSize);

    std::shared_ptr<CameraBuffer> pTop = std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, inSize, 0);
    EXPECT_NE(pTop, nullptr);

    icamera::camera_buffer_t bufInfo;
    bufInfo.s.memType = V4L2_MEMORY_DMABUF;
    bufInfo.addr = inBuf1;
    bufInfo.dmafd = inFd1;
    pTop->setUserBufferInfo(&bufInfo);

    int inFd2 = -1;
    void* inBuf2 = nullptr;
    ret = pipeline->createDMABuffer(inSize, &inFd2, &inBuf2);
    ASSERT_EQ(ret, 0);
    memcpy(inBuf2, bottom->getBufferAddr(), inSize);

    std::shared_ptr<CameraBuffer> pBottom = std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, inSize, 0);
    EXPECT_NE(pBottom, nullptr);

    bufInfo.addr = inBuf2;
    bufInfo.dmafd = inFd2;
    pBottom->setUserBufferInfo(&bufInfo);

    int dstFd = -1;
    void* dstBuf = nullptr;
    ret = pipeline->createDMABuffer(outSize, &dstFd, &dstBuf);
    ASSERT_EQ(ret, 0);

    std::shared_ptr<CameraBuffer> pDst = std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, outSize, 0);
    EXPECT_NE(pDst, nullptr);

    bufInfo.addr = dstBuf;
    bufInfo.dmafd = dstFd;
    pDst->setUserBufferInfo(&bufInfo);

    ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in = {pTop, pBottom};
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    util_write_to_file("cam_frame_512x512_dma_file_output.BIN8", static_cast<uint8_t*>(dstBuf), outSize);

    ret = verify_pixel_data(fmt, srcWidth, srcHeight,
                            static_cast<uint8_t*>(top->getBufferAddr()),
                            static_cast<uint8_t*>(bottom->getBufferAddr()),
                            static_cast<uint8_t*>(dstBuf));
    EXPECT_EQ(ret, 0) << "Fixel data incorrect." << "(" << srcWidth << "x" << srcHeight << ")";

    ret = destroyDMABuffer(inFd1, inBuf1);
    EXPECT_EQ(ret, 0);

    ret = destroyDMABuffer(inFd2, inBuf2);
    EXPECT_EQ(ret, 0);

    ret = destroyDMABuffer(dstFd, dstBuf);
    EXPECT_EQ(ret, 0);

    delete pipeline;
}

static void csc_pg_yuv420_to_rgbxxx(int width, int height, int dstFmt)
{
    if (!isPgIdSupported(CscPipeline::PG_ID)) return;

    string inFileName = "cam_frame_" + std::to_string(width) + "x" + std::to_string(height) + ".yuv420";
    // example:  "cam_frame_1920x1080.yuv420"

    size_t s;
    std::shared_ptr<CameraBuffer> pIn = util_get_file(inFileName.c_str(), &s);
    if (!pIn) {
        LOGD("@%s, fail to get the file for in frame, skip testing...", __func__);
        return;
    }

    CscPipeline* pipeline = new CscPipeline();

    int srcFmt = V4L2_PIX_FMT_YUV420;
    int dstSize = 0;
    string outFileName = "cam_frame_" + std::to_string(width) + "x" + std::to_string(height) + "_output.";
    dstSize = CameraUtils::getFrameSize(dstFmt, width, height);
    outFileName += CameraUtils::format2string(dstFmt);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, width, height, srcFmt);
    set_port_frame_info(dstFrame, MAIN_PORT, width, height, dstFmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    std::shared_ptr<CameraBuffer> pDst =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (dstSize + PAGE_SIZE), 0);
    EXPECT_NE(pDst, nullptr);

    int ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in(1, pIn);
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    util_write_to_file(outFileName.c_str(), static_cast<uint8_t*>(pDst->getBufferAddr()), dstSize);

    delete pipeline;
}

static void csc_pg_yuv420_to_rgbxxx_dma_in_dma_out(int width, int height, int dstFmt)
{
    int inFd = 0;
    int dstFd = 0;
    void* inBuf = nullptr;
    void* dstBuf = nullptr;
    int ret = 0;
    int srcSize = 0;
    if (!isPgIdSupported(CscPipeline::PG_ID)) return;

    string inFileName = "cam_frame_" + std::to_string(width) + "x" + std::to_string(height) + ".yuv420";
    // example:  "cam_frame_1920x1080.yuv420"

    size_t s;
    std::shared_ptr<CameraBuffer> pInData = util_get_file(inFileName.c_str(), &s);
    if (!pInData) {
        LOGD("@%s, fail to get the file for in frame, skip testing...", __func__);
        return;
    }

    TestCscPipeline *pipeline = new TestCscPipeline();

    int srcFmt = V4L2_PIX_FMT_YUV420;
    int dstSize = 0;
    string outFileName = "cam_frame_" + std::to_string(width) + "x" + std::to_string(height) + "_output.";
    dstSize = CameraUtils::getFrameSize(dstFmt, width, height);
    outFileName += CameraUtils::format2string(dstFmt);

    srcSize = CameraUtils::getFrameSize(srcFmt, width, height);
    ret= pipeline->createDMABuffer(srcSize, &inFd, &inBuf);
    ASSERT_EQ(ret, 0);
    memcpy(inBuf, pInData->getBufferAddr(), srcSize);

    ret = pipeline->createDMABuffer(dstSize, &dstFd, &dstBuf);
    ASSERT_EQ(ret, 0);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, width, height, srcFmt);
    set_port_frame_info(dstFrame, MAIN_PORT, width, height, dstFmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    std::shared_ptr<CameraBuffer> pIn = std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, srcSize, 0);
    EXPECT_NE(pIn, nullptr);

    icamera::camera_buffer_t bufInfo;
    bufInfo.s.memType = V4L2_MEMORY_DMABUF;
    bufInfo.addr = inBuf;
    bufInfo.dmafd = inFd;
    pIn->setUserBufferInfo(&bufInfo);

    std::shared_ptr<CameraBuffer> pDst =
        std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, dstSize, 0);
    EXPECT_NE(pDst, nullptr);

    bufInfo.addr = dstBuf;
    bufInfo.dmafd = dstFd;
    pDst->setUserBufferInfo(&bufInfo);

    vector<std::shared_ptr<CameraBuffer>> in(1, pIn);
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);

    EXPECT_EQ(ret, 0);

    util_write_to_file(outFileName.c_str(), static_cast<uint8_t*>(dstBuf), dstSize);

    ret = destroyDMABuffer(inFd, inBuf);
    EXPECT_EQ(ret, 0);

    ret = destroyDMABuffer(dstFd, dstBuf);
    EXPECT_EQ(ret, 0);

    delete pipeline;
}

static void scale_pg_yuv422_buffer_in_dma_out(int srcWidth, int srcHeight, int srcFmt, int dstWidth,
            int dstHeight, int dstFmt)
{
    if (!isPgIdSupported(ScalePipeline::PG_ID)) return;

    string inFileName = "IMAGE_" + std::to_string(srcWidth) + "x" + std::to_string(srcHeight) + "_" + CameraUtils::format2string(srcFmt) + "_8b.bin";

    size_t s;
    std::shared_ptr<CameraBuffer> pIn = util_get_file(inFileName.c_str(), &s);
    if (!pIn) {
        LOGD("@%s, fail to get the file for in frame, skip testing...", __func__);
        return;
    }

    int srcSize = CameraUtils::getFrameSize(srcFmt, srcWidth, dstHeight);
    if (s != srcSize) {
        LOGD("@%s, input file size doesn't meet the requirement", __func__);
        return;
    }
    TestScalePipeline *pipeline = new TestScalePipeline();
    int dstSize = CameraUtils::getFrameSize(dstFmt, dstWidth, dstHeight);
    int dstFd = 0;
    void* dstBuf = nullptr;
    int ret = pipeline->createDMABuffer(dstSize, &dstFd, &dstBuf);
    ASSERT_EQ(ret, 0);

    std::shared_ptr<CameraBuffer> pDst =
        std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, dstSize, 0);
    EXPECT_NE(pDst, nullptr);

    icamera::camera_buffer_t bufInfo;
    bufInfo.s.memType = V4L2_MEMORY_DMABUF;
    bufInfo.addr = dstBuf;
    bufInfo.dmafd = dstFd;
    pDst->setUserBufferInfo(&bufInfo);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcWidth, srcHeight, srcFmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstWidth, dstHeight, dstFmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in(1, pIn);
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);

    EXPECT_EQ(ret, 0);

    string outFileName = "cam_frame_" + std::to_string(dstWidth) + "x" + std::to_string(dstHeight) + "_scale_output.";
    outFileName += CameraUtils::format2string(dstFmt);
    util_write_to_file(outFileName.c_str(), static_cast<uint8_t*>(dstBuf), dstSize);

    ret = destroyDMABuffer(dstFd, dstBuf);
    EXPECT_EQ(ret, 0);

    delete pipeline;
}

static void scale_pg_yuv422_dma_in_dma_out(int srcWidth, int srcHeight, int srcFmt, int dstWidth,
            int dstHeight, int dstFmt)
{

    if (!isPgIdSupported(ScalePipeline::PG_ID)) return;

    string inFileName = "IMAGE_" + std::to_string(srcWidth) + "x" + std::to_string(srcHeight) + "_" + CameraUtils::format2string(srcFmt) + "_8b.bin";

    size_t s;
    std::shared_ptr<CameraBuffer> pInData = util_get_file(inFileName.c_str(), &s);
    if (!pInData) {
        LOGD("@%s, fail to get the file for in frame, skip testing...", __func__);
        return;
    }

    TestScalePipeline *pipeline = new TestScalePipeline();
    int srcSize = CameraUtils::getFrameSize(srcFmt, srcWidth, dstHeight);
    int inFd = 0;
    void* inBuf = nullptr;
    int ret = 0;
    ret= pipeline->createDMABuffer(srcSize, &inFd, &inBuf);
    ASSERT_EQ(ret, 0);
    memcpy(inBuf, pInData->getBufferAddr(), srcSize);

    std::shared_ptr<CameraBuffer> pIn =
        std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, srcSize, 0);
    EXPECT_NE(pIn, nullptr);

    icamera::camera_buffer_t bufInfo;
    bufInfo.s.memType = V4L2_MEMORY_DMABUF;
    bufInfo.addr = inBuf;
    bufInfo.dmafd = inFd;
    pIn->setUserBufferInfo(&bufInfo);

    int dstSize = CameraUtils::getFrameSize(dstFmt, dstWidth, dstHeight);
    int dstFd = 0;
    void* dstBuf = nullptr;
    ret = pipeline->createDMABuffer(dstSize, &dstFd, &dstBuf);
    ASSERT_EQ(ret, 0);

    std::shared_ptr<CameraBuffer> pDst =
        std::make_shared<CameraBuffer>(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_DMABUF, dstSize, 0);
    EXPECT_NE(pDst, nullptr);

    bufInfo.addr = inBuf;
    bufInfo.dmafd = inFd;
    pDst->setUserBufferInfo(&bufInfo);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcWidth, srcHeight, srcFmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstWidth, dstHeight, dstFmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in(1, pIn);
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst);

    ret = pipeline->iterate(in, out);

    EXPECT_EQ(ret, 0);

    string outFileName = "cam_frame_" + std::to_string(dstWidth) + "x" + std::to_string(dstHeight) + "_scale_output.";
    outFileName += CameraUtils::format2string(dstFmt);
    util_write_to_file(outFileName.c_str(), static_cast<uint8_t*>(dstBuf), dstSize);

    ret = destroyDMABuffer(inFd, inBuf);
    EXPECT_EQ(ret, 0);

    ret = destroyDMABuffer(dstFd, dstBuf);
    EXPECT_EQ(ret, 0);

    delete pipeline;
}


TEST(camCiprTest, scale_pg_yuv422_to_NV12_1080p_to_720p_dma_in_dma_out)
{
    scale_pg_yuv422_dma_in_dma_out(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, scale_pg_yuv422_to_yuv420_1080p_to_720p_dma_in_dma_out)
{
    scale_pg_yuv422_dma_in_dma_out(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, scale_pg_yuv422_to_NV12_1080p_to_720p_buffer_in_dma_out)
{
    scale_pg_yuv422_buffer_in_dma_out(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, scale_pg_yuv422_to_yuv420_1080p_to_720p_buffer_in_dma_out)
{
    scale_pg_yuv422_buffer_in_dma_out(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}


TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgb565_1080p_dma_in_dma_out)
{
    csc_pg_yuv420_to_rgbxxx_dma_in_dma_out(1920, 1080, V4L2_PIX_FMT_RGB565);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgb565_720p_dma_in_dma_out)
{
    csc_pg_yuv420_to_rgbxxx_dma_in_dma_out(1280, 720, V4L2_PIX_FMT_RGB565);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgb888_1080p_dma_in_dma_out)
{
    csc_pg_yuv420_to_rgbxxx_dma_in_dma_out(1920, 1080, V4L2_PIX_FMT_RGB24);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgba888_1080p_dma_in_dma_out)
{
    csc_pg_yuv420_to_rgbxxx_dma_in_dma_out(1920, 1080, V4L2_PIX_FMT_RGB32);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgb565_720p)
{
    csc_pg_yuv420_to_rgbxxx(1280, 720, V4L2_PIX_FMT_RGB565);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgb565_1080p)
{
    csc_pg_yuv420_to_rgbxxx(1920, 1080, V4L2_PIX_FMT_RGB565);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgb888_1080p)
{
    csc_pg_yuv420_to_rgbxxx(1920, 1080, V4L2_PIX_FMT_RGB24);
}

TEST(camCiprTest, yuv_csc_pg_yuv420_to_rgba888_1080p)
{
    csc_pg_yuv420_to_rgbxxx(1920, 1080, V4L2_PIX_FMT_RGB32);
}

TEST_F(camHalTest, csc_full_pipe_output_rgb24)
{
    // This case involves CSC PG, we need to make sure the FW contains the PG.
    if (!isPgIdSupported(CscPipeline::PG_ID)) return;

    test_configure_with_input_format(V4L2_PIX_FMT_SGRBG8V32, V4L2_PIX_FMT_RGB24, 1920, 1080);
}

static void scale_pg_yuv422_to_YUV420(int srcwidth, int srcheight, int srcFmt, int dstwidthdp, int dstheightdp, int dstFmtdp, int dstwidthmp=0, int dstheightmp=0, int dstFmtmp=0)
{
    string inFileName = "IMAGE_" + std::to_string(srcwidth) + "x" + std::to_string(srcheight) + "_" + CameraUtils::format2string(srcFmt) + "_8b.bin";
    // example:  "IMAGE_640x480_YUYV_8b.bin"

    size_t s;
    std::shared_ptr<CameraBuffer> pIn = util_get_file(inFileName.c_str(), &s);
    if (!pIn) {
        LOGD("@%s, fail to get the file for in frame, skip testing...", __func__);
        return;
    }

    int out_pins = 1;
    if (dstwidthmp != 0 && dstheightmp != 0)
        out_pins = 2;

    ScalePipeline* pipeline = new ScalePipeline();

    string outFileNameDP = "cam_frame_" + std::to_string(srcwidth) + "x" + std::to_string(srcheight) + "_in_" + std::to_string(dstwidthdp) + "x" + std::to_string(dstheightdp) + "_scale_output_" + std::to_string(out_pins) + "pins_DP.";
    int dstSizeDP = CameraUtils::getFrameSize(dstFmtdp, dstwidthdp, dstheightdp);
    outFileNameDP += CameraUtils::format2string(dstFmtdp);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcwidth, srcheight, srcFmt);

    pipeline->setInputInfo(srcFrame);

    set_port_frame_info(dstFrame, MAIN_PORT, dstwidthdp, dstheightdp, dstFmtdp);

    std::shared_ptr<CameraBuffer> pDst1 =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (dstSizeDP + PAGE_SIZE), 0);
    EXPECT_NE(pDst1, nullptr);

    string outFileNameMP;
    int dstSizeMP = 0;
    std::shared_ptr<CameraBuffer> pDst2;
    if (dstwidthmp != 0 && dstheightmp != 0) {
        outFileNameMP = "cam_frame_" + std::to_string(srcwidth) + "x" + std::to_string(srcheight) + "_in_" + std::to_string(dstwidthmp) + "x" + std::to_string(dstheightmp) + "_scale_output_" + std::to_string(out_pins) + "pins_MP.";
        dstSizeMP = CameraUtils::getFrameSize(dstFmtmp, dstwidthmp, dstheightmp);
        outFileNameMP += CameraUtils::format2string(dstFmtmp);

        set_port_frame_info(dstFrame, SECOND_PORT, dstwidthmp, dstheightmp, dstFmtmp);
    }

    pipeline->setOutputInfo(dstFrame);

    if (dstwidthmp != 0 && dstheightmp != 0) {
        pDst2 = CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (dstSizeMP + PAGE_SIZE), 0);
        EXPECT_NE(pDst2, nullptr);
    }

    int ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in(1, pIn);
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst1);
    if (dstwidthmp != 0 && dstheightmp != 0)
        out.push_back(pDst2);

    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    // current Scale PG only enable Dout0
    util_write_to_file(outFileNameDP.c_str(), static_cast<uint8_t*>(pDst1->getBufferAddr()), dstSizeDP);
    if (dstwidthmp != 0 && dstheightmp != 0)
        util_write_to_file(outFileNameMP.c_str(), static_cast<uint8_t*>(pDst2->getBufferAddr()), dstSizeMP);

    delete pipeline;
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_1p5x)
{
    scale_pg_yuv422_to_YUV420(1280, 960, V4L2_PIX_FMT_YUYV, 1920, 1440, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_1p5x_2pins)
{
    scale_pg_yuv422_to_YUV420(1280, 960, V4L2_PIX_FMT_YUYV, 1920, 1440, V4L2_PIX_FMT_YUV420, 1280, 960, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_2x)
{
    scale_pg_yuv422_to_YUV420(2048, 1536, V4L2_PIX_FMT_YUYV, 4096, 3072, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_2x_2pins)
{
    scale_pg_yuv422_to_YUV420(2048, 1536, V4L2_PIX_FMT_YUYV, 4096, 3072, V4L2_PIX_FMT_YUV420, 2048, 1536, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_2p5x)
{
    scale_pg_yuv422_to_YUV420(1280, 960, V4L2_PIX_FMT_YUYV, 3200, 2400, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_2p5x_2pins)
{
    scale_pg_yuv422_to_YUV420(1280, 960, V4L2_PIX_FMT_YUYV, 3200, 2400, V4L2_PIX_FMT_YUV420, 1280, 960, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_3x)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1440, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Up_3x_2pins)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1440, V4L2_PIX_FMT_YUV420, 640, 480, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_1p5x)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_1p5x_2pins)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_2x)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 1280, 960, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_2x_2pins)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 1280, 960, V4L2_PIX_FMT_YUV420, 2560, 1920, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_3x)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 360, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_3x_2pins)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 360, V4L2_PIX_FMT_YUV420, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_4x)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_4x_2pins)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420, 2560, 1920, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_5x)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 512, 384, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_5x_2pins)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 512, 384, V4L2_PIX_FMT_YUV420, 2560, 1920, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_6x)
{
    scale_pg_yuv422_to_YUV420(3840, 2160, V4L2_PIX_FMT_YUYV, 640, 360, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_6x_2pins)
{
    scale_pg_yuv422_to_YUV420(3840, 2160, V4L2_PIX_FMT_YUYV, 640, 360, V4L2_PIX_FMT_YUV420, 3840, 2160, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_8x)
{
    scale_pg_yuv422_to_YUV420(4096, 3072, V4L2_PIX_FMT_YUYV, 512, 384, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_YUV420_Down_8x_2pins)
{
    scale_pg_yuv422_to_YUV420(4096, 3072, V4L2_PIX_FMT_YUYV, 512, 384, V4L2_PIX_FMT_YUV420, 4096, 3072, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_to_NV12_1080P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Up_1p5x)
{
    scale_pg_yuv422_to_YUV420(1280, 960, V4L2_PIX_FMT_YUYV, 1920, 1440, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Up_2x)
{
    scale_pg_yuv422_to_YUV420(2048, 1536, V4L2_PIX_FMT_YUYV, 4096, 3072, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Up_2p5x)
{
    scale_pg_yuv422_to_YUV420(1280, 960, V4L2_PIX_FMT_YUYV, 3200, 2400, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Up_3x)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1440, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_1p5x)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_2x)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 1280, 960, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_3x)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 360, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_4x)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_5x)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 512, 384, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_6x)
{
    scale_pg_yuv422_to_YUV420(3840, 2160, V4L2_PIX_FMT_YUYV, 640, 360, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_to_NV12_Down_8x)
{
    scale_pg_yuv422_to_YUV420(4096, 3072, V4L2_PIX_FMT_YUYV, 512, 384, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_720P_to_YUV420_720P)
{
    scale_pg_yuv422_to_YUV420(1280, 720, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_UYVY_VGA_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_UYVY, 640, 480, V4L2_PIX_FMT_YUV420);
}
//VGA Input
TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_to_YUV420_720P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_UYVY_VGA_to_YUV420_720P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_UYVY_VGA_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_UYVY, 1920, 1080, V4L2_PIX_FMT_YUV420);
}
//720x576 Input
TEST(camCiprTest, yuv_scale_pg_YUYV_720x576_to_YUV420_720P)
{
    scale_pg_yuv422_to_YUV420(720, 576, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_720x576_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(720, 576, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_720x576_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(720, 576, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}
//720p Input
TEST(camCiprTest, yuv_scale_pg_YUYV_720P_to_YUV420_4k)
{
    scale_pg_yuv422_to_YUV420(1280, 720, V4L2_PIX_FMT_YUYV, 3840, 2160, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_720P_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(1280, 720, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_720P_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(1280, 720, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}
//1080p Input
TEST(camCiprTest, yuv_scale_pg_YUYV_1080p_to_YUV420_4k)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 3840, 2160, V4L2_PIX_FMT_YUV420);
}
TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_to_YUV420_720P)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}
//480p Input
TEST(camCiprTest, yuv_scale_pg_YUYV_480P_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(720, 480, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_480P_to_YUV420_720P)
{
    scale_pg_yuv422_to_YUV420(720, 480, V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_480P_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(720, 480, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}
//scale ratio test
TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_to_YUV420_2560x1920)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 2560, 1920, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_2560x1920_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(2560, 1920, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}
//for crop test
TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_crop_to_YUV420_VGA)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_crop_to_NV12_VGA)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_1080P_crop_to_NV21_VGA)
{
    scale_pg_yuv422_to_YUV420(1920, 1080, V4L2_PIX_FMT_YUYV, 640, 480, V4L2_PIX_FMT_NV21);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_crop_to_YUV420_1080P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_YUV420);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_crop_to_NV12_1080P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_NV12);
}

TEST(camCiprTest, yuv_scale_pg_YUYV_VGA_crop_to_NV21_1080P)
{
    scale_pg_yuv422_to_YUV420(640, 480, V4L2_PIX_FMT_YUYV, 1920, 1080, V4L2_PIX_FMT_NV21);
}

TEST_F(camHalTest, Scale_full_pipe_output_1080P)
{
    // This case involves Scaling PG, we need to make sure the FW contains the PG.
    if (!isPgIdSupported(ScalePipeline::PG_ID)) return;
    test_configure_with_input_size(1920, 1080, 1920, 1080);
}

TEST_F(camHalTest, Scale_full_pipe_output_VGA)
{
    // This case involves Scaling PG, we need to make sure the FW contains the PG.
    if (!isPgIdSupported(ScalePipeline::PG_ID)) return;
    test_configure_with_input_size(640, 480, 1920, 1080);
}

static void fisheye_pg_yuv422_to_yuv422(int srcFmt, int srcwidth, int srcheight, int dstFmt, int dstwidth, int dstheight,
    camera_fisheye_dewarping_mode_t dewarpingMode)
{
    if (!isPgIdSupported(FisheyePipeline::PG_ID)) return;

    string inFileName = "IMAGE_" + std::to_string(srcwidth) + "x" + std::to_string(srcheight) + "_" + CameraUtils::format2string(srcFmt) + "_8b.bin";
    // example:  "IMAGE_1280x720_YUYV_8b.bin"

    size_t s;
    std::shared_ptr<CameraBuffer> pIn = util_get_file(inFileName.c_str(), &s);
    if (!pIn) {
        LOGD("@%s, fail to get the file for in frame, skip testing...", __func__);
        return;
    }

    int cameraId = getCurrentCameraId();
    FisheyePipeline* pipeline = new FisheyePipeline(cameraId);

    string outFileName = "cam_frame_" + std::to_string(srcwidth) + "x" + std::to_string(srcheight) + "_in_" + std::to_string(dstwidth) + "x" + std::to_string(dstheight) + "_dewarping_output_";
    int dstSize = CameraUtils::getFrameSize(dstFmt, dstwidth, dstheight);

    FrameInfoPortMap srcFrame;
    FrameInfoPortMap dstFrame;

    set_port_frame_info(srcFrame, MAIN_PORT, srcwidth, srcheight, srcFmt);
    set_port_frame_info(dstFrame, MAIN_PORT, dstwidth, dstheight, dstFmt);

    pipeline->setInputInfo(srcFrame);
    pipeline->setOutputInfo(dstFrame);

    std::shared_ptr<CameraBuffer> pDst1 =
        CameraBuffer::create(0, BUFFER_USAGE_PSYS_INPUT, V4L2_MEMORY_USERPTR, (dstSize + PAGE_SIZE), 0);
    EXPECT_NE(pDst1, nullptr);

    Parameters *param = new Parameters;
    param->setFisheyeDewarpingMode(dewarpingMode);
    pipeline->setParameters(*param);
    if (dewarpingMode == FISHEYE_DEWARPING_REARVIEW)
        outFileName += "rearview.";
    else if (dewarpingMode == FISHEYE_DEWARPING_HITCHVIEW)
        outFileName += "hitchview.";
    outFileName += CameraUtils::format2string(dstFmt);

    int ret = pipeline->prepare();
    EXPECT_EQ(ret, 0);

    vector<std::shared_ptr<CameraBuffer>> in(1, pIn);
    vector<std::shared_ptr<CameraBuffer>> out(1, pDst1);

    ret = pipeline->iterate(in, out);
    EXPECT_EQ(ret, 0);

    util_write_to_file(outFileName.c_str(), static_cast<uint8_t*>(pDst1->getBufferAddr()), dstSize);

    delete param;
    delete pipeline;
}

//720p->1280x768
TEST(camCiprTest, fisheye_pg_YUYV_720p_to_YUYV_1280_768_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_YUYV_1280_768_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_UYVY_1280_768_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_UYVY_1280_768_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_HITCHVIEW);
}

//1280x768->1280x768
TEST(camCiprTest, fisheye_pg_YUYV_1280_768_to_YUYV_1280_768_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 768, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_1280_768_to_YUYV_1280_768_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 768, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_1280_768_to_UYVY_1280_768_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 768, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_1280_768_to_UYVY_1280_768_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 768, V4L2_PIX_FMT_YUYV, 1280, 768, FISHEYE_DEWARPING_HITCHVIEW);
}

//720p->1920x1088
TEST(camCiprTest, fisheye_pg_YUYV_720p_to_YUYV_1920_1088_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_YUYV_1920_1088_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_UYVY_1920_1088_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_UYVY_1920_1088_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_HITCHVIEW);
}

//1920x1088->1920x1088
TEST(camCiprTest, fisheye_pg_YUYV_1920_1088_to_YUYV_1920_1088_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1920, 1088, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_1920_1088_to_YUYV_1920_1088_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1920, 1088, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_1920_1088_to_UYVY_1920_1088_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1920, 1088, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_1920_1088_to_UYVY_1920_1088_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1920, 1088, V4L2_PIX_FMT_YUYV, 1920, 1088, FISHEYE_DEWARPING_HITCHVIEW);
}

//720p->896x480
TEST(camCiprTest, fisheye_pg_YUYV_720p_to_YUYV_896_480_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_YUYV_896_480_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 1280, 720, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_UYVY_896_480_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_720p_to_UYVY_896_480_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 1280, 720, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_HITCHVIEW);
}

//896x480->896x480
TEST(camCiprTest, fisheye_pg_YUYV_896_480_to_YUYV_896_480_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 896, 480, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_896_480_to_YUYV_896_480_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_YUYV, 896, 480, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_896_480_to_UYVY_896_480_rearview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 896, 480, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_REARVIEW);
}

TEST(camCiprTest, fisheye_pg_YUYV_896_480_to_UYVY_896_480_hitchview) {
    fisheye_pg_yuv422_to_yuv422(V4L2_PIX_FMT_UYVY, 896, 480, V4L2_PIX_FMT_YUYV, 896, 480, FISHEYE_DEWARPING_HITCHVIEW);
}

TEST_F(camHalTest, camera_device_configure_with_input_format)
{
    // This case involves Scaling and CSC PG, we need to make sure the FW contains the PGs.
    if (!isPgIdSupported(ScalePipeline::PG_ID) || !isPgIdSupported(CscPipeline::PG_ID)) {
        return;
    }

    test_configure_with_input_format(V4L2_PIX_FMT_SGRBG8V32, V4L2_PIX_FMT_NV12, 1920, 1080);
}

