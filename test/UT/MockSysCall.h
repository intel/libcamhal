/*
 * Copyright (C) 2015-2018 Intel Corporation.
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

#include "gmock/gmock.h"
#include "SysCall.h"

using namespace icamera;

class MockSysCall: public SysCall {

    public:
        MockSysCall();
        ~MockSysCall();

    public:
        MOCK_METHOD2(open, int(const char*, int));
        MOCK_METHOD1(close, int(int));

        MOCK_METHOD3(ioctl, int(int, int, struct media_device_info *));
        MOCK_METHOD3(ioctl, int(int, int, struct media_link_desc *));

        MOCK_METHOD3(ioctl, int(int, int, struct media_links_enum *));
        MOCK_METHOD3(ioctl, int(int, int, struct media_links_desc *));
        MOCK_METHOD3(ioctl, int(int, int, struct media_entity_desc *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_capability *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_exportbuffer *));
        MOCK_METHOD3(ioctl, int(int, int, v4l2_fmtdesc *));
        MOCK_METHOD3(ioctl, int(int, int, enum v4l2_buf_type *));
        //MOCK_METHOD3(ioctl, int(int, int, struct v4l2_format *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_requestbuffers *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_buffers *));
        //MOCK_METHOD3(ioctl, int(int, int, struct v4l2_buffer *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_subdev_format *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_control *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_queryctrl *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_subdev_selection*));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_querymenu*));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_event_subscription *));
        MOCK_METHOD3(ioctl, int(int, int, struct v4l2_subdev_routing*));

        int ioctl(int fd, int request, struct v4l2_buffer *arg);
        int ioctl(int fd, int request, struct v4l2_event *arg);
        int ioctl(int fd, int request, struct v4l2_format *format);
        int poll(struct pollfd *pfd, nfds_t nfds, int timeout);
        void *mmap(void *addr, size_t len, int prot, int flag, int filedes, off_t off);
        int munmap(void *addr, size_t len);

    public:
        void mockOpen();
        void mockClose();
        void mockIoctl();
        void unMockIoctl();

    private:
        int mockMediaCtlIoctl();
        int mockMediaCtlIoctl_MEDIA_IOC_DEVICE_INFO();
        int mockMediaCtlIoctl_MEDIA_IOC_ENUM_ENTITIES();
        int mockMediaCtlIoctl_MEDIA_IOC_ENUM_LINKS();
        int mockMediaCtlIoctl_MEDIA_IOC_SETUP_LINK();
        int mockV4l2DevIoctl();
        int mockV4l2DevIoctl_VIDIOC_QUERYCAP();
        int mockV4l2DevIoctl_VIDIOC_ENUM_FMT();
        int mockV4l2DevIoctl_VIDIOC_EXPBUF();
        int mockV4l2SubDevIoctl();
        int mockV4l2SubDevIoctl_VIDIOC_S_CTRL();
        int mockV4l2SubDevIoctl_VIDIOC_QUERYCTRL();
        int mockV4l2SubDevIoctl_VIDIOC_SUBDEV_S_SELECTION();
        int readFileImgIntoBuf(char* destBuf, int bufSize, int index);


        void setup(struct media_device_info &info, const char *driver, const char *model, const char *serial, const char *bus_info, const int media_version, const int hw_revision, const int driver_version);
        void setup(struct media_entity_desc &desc, const int id, const char * name, const int type, const int revision, const int flags, const int group_id, const int pads, const int links);

        void setup(struct media_links_enum &link, const int entity, const struct media_pad_desc *pads, const int npads, const struct media_link_desc *links, const int nlinks, const unsigned int *reserved);

        void setup(struct media_pad_desc &pad, const unsigned int entity, const unsigned short index, const unsigned int flags, const unsigned int *reserved);

        void setup(struct media_link_desc &linkDesc, const struct media_pad_desc &source, const struct media_pad_desc &sink, const unsigned int flags, const unsigned int *reserved);

    private:
        int mMediaCtlFd;
        int mV4l2SubDevFd;
        int mV4l2DevFd;
        struct v4l2_format mV4l2DevFmt;
        int mImgDataIndex;

        struct media_links_enum mLinkEnum[32];
};

#ifdef MOCK_TEST //run test cases on any linux
using testing::NiceMock;
// Tests non-fatal failures in the fixture constructor.
class camHalTest: public testing::Test {
    protected:
        camHalTest() {
            mMockSysCall = new NiceMock<MockSysCall>;
            SysCall::updateInstance(mMockSysCall);
        }

        ~camHalTest() {
            delete mMockSysCall;
            mMockSysCall = NULL;
            SysCall::updateInstance(NULL);
        }

        virtual void SetUp() {
            mMockSysCall->mockOpen();
            mMockSysCall->mockIoctl();
            mMockSysCall->mockClose();
        }

        virtual void TearDown() {
            mMockSysCall->unMockIoctl();
        }
    public:
        NiceMock<MockSysCall> *mMockSysCall;
};
#else //run test cases on target device with real sensor.
class camHalTest: public testing::Test {

};
#endif //MOCK_TEST
