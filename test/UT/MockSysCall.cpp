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

#include "gtest/gtest.h"
#include "gmock/gmock.h"
#include "linux/v4l2-subdev.h"
#include "MockSysCall.h"
#include "MediaControl.h"
#include "SysCall.h"
#include "stdio.h"

#define LOG_TAG "MockSysCall"

#include <iutils/CameraLog.h>
#include <iutils/Utils.h>
using namespace icamera;

using testing::_;
using testing::A;
using testing::SetArgReferee;
using testing::SetArgPointee;
using testing::SetErrnoAndReturn;
using testing::Lt;
using testing::StrNe;
using testing::StrEq;
using testing::Return;

#define FPS_200_INTERVAL 50000
#define FILE_NAME_LENGTH 64

MockSysCall::MockSysCall() : mMediaCtlFd(-1),
                             mV4l2SubDevFd(-1),
                             mV4l2DevFd(-1)
{
    LOG1("@%s", __func__);
    memset(mLinkEnum, 0, sizeof(media_links_enum) * 32);
    memset(&mV4l2DevFmt, 0, sizeof(v4l2_format));
    mImgDataIndex = 0;
}

MockSysCall::~MockSysCall()
{
    LOG1("@%s", __func__);
}

const int MEDIACTL_FD = 100;
const int V4L2SUBDEV_FD = 200;
const int V4L2DEV_FD = 300;
const char *MEDIACTL_NAME = "/dev/media0";
const char *V4L2DEV_NAME = "/dev/video5";
const char *V4L2DEV_NAME_2 = "/dev/video0";


void MockSysCall::mockOpen()
{
    mMediaCtlFd = MEDIACTL_FD;
    mV4l2SubDevFd = V4L2SUBDEV_FD;
    mV4l2DevFd = V4L2DEV_FD;

    ON_CALL(*this, open(StrEq(MEDIACTL_NAME), O_RDWR)).WillByDefault(Return(mMediaCtlFd));
    ON_CALL(*this, open(StrEq(V4L2DEV_NAME), O_RDWR)).WillByDefault(Return(mV4l2DevFd));
    ON_CALL(*this, open(StrEq(V4L2DEV_NAME_2), O_RDWR)).WillByDefault(Return(mV4l2DevFd));
    //ON_CALL(*this, open(StrEq(V4L2SUBDEV_NAME), O_RDWR)).WillByDefault(Return(mV4l2SubDevFd));
}

void MockSysCall::mockClose()
{
    ON_CALL(*this, close(Lt(V4L2DEV_FD))).WillByDefault(Return(0));
}

#define cpyStr(buf, str) \
    do { \
        int len = strlen(str); \
        memcpy(buf, str, len); \
        buf[len] = '\0';       \
   } while (0)

void MockSysCall::mockIoctl()
{
    mockMediaCtlIoctl();
    mockV4l2DevIoctl();
    mockV4l2SubDevIoctl();
}

void MockSysCall::unMockIoctl()
{
}

void MockSysCall::setup(struct media_device_info &info, const char *driver, const char *model, const char *serial, const char *bus_info, const int media_version, const int hw_revision, const int driver_version)
{
    memset(&info, 0, sizeof(media_device_info));
    cpyStr(info.driver, driver);
    cpyStr(info.model, model);
    cpyStr(info.serial, serial);
    cpyStr(info.bus_info, bus_info);
    info.media_version = media_version;
    info.hw_revision = hw_revision;
    info.driver_version = driver_version;
}

void MockSysCall::setup(struct media_entity_desc &desc, const int id, const char* name, const int type, const int revision, const int flags, const int group_id, const int pads, const int links)
{
    memset(&desc, 0, sizeof(media_entity_desc));
    desc.id = id;
    cpyStr(desc.name, name);
    desc.type = type;
    desc.revision = revision;
    desc.flags = flags;
    desc.group_id = group_id;
    desc.pads = pads;
    desc.links = links;
}

void MockSysCall::setup(struct media_pad_desc &pad, const unsigned int entity, const unsigned short index, const unsigned int flags, const unsigned int *reserved)
{
    memset(&pad, 0, sizeof(media_pad_desc));
    pad.entity = entity;
    pad.index = index;
    pad.flags = flags;
    pad.reserved[0] = reserved[0];
    pad.reserved[1] = reserved[1];
}

void MockSysCall::setup(struct media_link_desc &linkDesc, const struct media_pad_desc &source, const struct media_pad_desc &sink, const unsigned int flags, const unsigned int *reserved)
{
    memset(&linkDesc, 0, sizeof(struct media_link_desc));
    memcpy(&linkDesc.source, &source, sizeof(struct media_pad_desc));
    memcpy(&linkDesc.sink, &sink, sizeof(struct media_pad_desc));
    linkDesc.flags = flags;
    linkDesc.reserved[0] = reserved[0];
    linkDesc.reserved[1] = reserved[1];
}

void MockSysCall::setup(struct media_links_enum &link, const int entity, const struct media_pad_desc *pads, const int npads, const struct media_link_desc *links, const int nlinks, const unsigned int *reserved)
{
    memset(&link, 0, sizeof(struct media_links_enum));
    link.pads = new media_pad_desc[npads];
    link.links = new media_link_desc [nlinks];
    link.entity = entity;
    memcpy(link.pads, pads, sizeof(struct media_pad_desc) * npads);
    memcpy(link.links, links, sizeof(struct media_link_desc) * nlinks);
    link.reserved[0] = reserved[0];
    link.reserved[1] = reserved[1];
    link.reserved[2] = reserved[2];
    link.reserved[3] = reserved[3];
}

int MockSysCall::mockMediaCtlIoctl_MEDIA_IOC_DEVICE_INFO()
{
    /* mock MEDIA_IOC_DEVICE_INFO ioctl for media ctl */
    media_device_info info;

    setup(info, "intel-ipu4-isys", "ipu4/Broxton B", "", "pci:0000:00:03.0", 256,256,0);

    ON_CALL(*this,  ioctl(MEDIACTL_FD, MEDIA_IOC_DEVICE_INFO, A<struct media_device_info *>())).WillByDefault(DoAll(SetArgPointee<2>(info), Return(0)));

    return 0;
}

int MockSysCall::mockMediaCtlIoctl_MEDIA_IOC_ENUM_ENTITIES()
{
    /* mock MEDIA_IOC_ENUM_ENTITIES ioctl for media ctl */
    struct media_entity_desc entityDesc[32];

    setup(entityDesc[0], 1, "Intel IPU4 CSI-2 0", 0x20000, 0, 0, 0, 2, 3);
    setup(entityDesc[1], 2, "Intel IPU4 CSI-2 0 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[2], 3, "Intel IPU4 CSI-2 1", 0x20000, 0, 0, 0, 2, 3);
    setup(entityDesc[3], 4, "Intel IPU4 CSI-2 1 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[4], 5, "Intel IPU4 CSI-2 2", 0x20000, 0, 0, 0, 2, 3);
    setup(entityDesc[5], 6, "Intel IPU4 CSI-2 2 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[6], 7, "Intel IPU4 CSI-2 3", 0x20000, 0, 0, 0, 2, 3);
    setup(entityDesc[7], 8, "Intel IPU4 CSI-2 3 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[8], 9, "Intel IPU4 CSI-2 4", 0x20000, 0, 0, 0, 2, 3);
    setup(entityDesc[9], 10, "Intel IPU4 CSI-2 4 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[10], 11, "Intel IPU4 CSI-2 5", 0x20000, 0, 0, 0, 2, 3);
    setup(entityDesc[11], 12, "Intel IPU4 CSI-2 5 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[12], 13, "Intel IPU4 TPG 0", 0x20000, 0, 0, 0, 1, 2);
    setup(entityDesc[13], 14, "Intel IPU4 TPG 0 capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[14], 15, "Intel IPU4 CSI2 BE", 0x20000, 0, 0, 0, 2, 2);
    setup(entityDesc[15], 16, "Intel IPU4 CSI2 BE capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[16], 17, "Intel IPU4 CSI2 BE SOC", 0x20000, 0, 0, 0, 2, 1);
    setup(entityDesc[17], 18, "Intel IPU4 CSI2 BE SOC capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[18], 19, "Intel IPU4 ISA", 0x20000, 0, 0, 0, 5, 3);
    setup(entityDesc[19], 20, "Intel IPU4 ISA capture", 0x10001, 0, 0, 0, 1, 0);
    setup(entityDesc[20], 21, "Intel IPU4 ISA config", 0x10001, 0, 0, 0, 1, 1);
    setup(entityDesc[21], 22, "Intel IPU4 ISA 3A stats", 0x10001, 0, 0, 0, 1, 0);
    setup(entityDesc[22], 23, "Intel IPU4 ISA scaled capture", 0x10001, 0, 0, 0, 1, 0);

    setup(entityDesc[23], 24, "ov13860 pixel array 2-0010", 0x20001, 0, 0, 0, 1, 1);
    setup(entityDesc[24], 25, "ov13860 binner 2-0010", 0x20000, 0, 0, 0, 2, 1);

    setup(entityDesc[25], 26, "adv7481 pixel array 2-00e0", 0x20001, 0, 0, 0, 1, 1);
    setup(entityDesc[26], 27, "adv7481 binner 2-00e0", 0x20000, 0, 0, 0, 2, 1);

    EXPECT_CALL(*this,  ioctl(MEDIACTL_FD, MEDIA_IOC_ENUM_ENTITIES, A<struct media_entity_desc*>()))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[0]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[1]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[2]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[3]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[4]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[5]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[6]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[7]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[8]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[9]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[10]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[11]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[12]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[13]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[14]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[15]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[16]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[17]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[18]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[19]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[20]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[21]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[22]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[23]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[24]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[25]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(entityDesc[26]), Return(0)))
        .WillRepeatedly(SetErrnoAndReturn(EINVAL, -1));

    return 0;
}

#define MAX_ENTITY 24
#define MAX_PADS 4
#define MAX_LINKS 4

#define setup_link_enum(entity, npads, nlinks) \
    setup(mLinkEnum[entity-1], entity, pads_##entity, npads, links_##entity, nlinks, linksEnum_reserved);

int MockSysCall::mockMediaCtlIoctl_MEDIA_IOC_ENUM_LINKS()
{
    const unsigned int linksEnum_reserved[4] = {0, 0, 0, 0};

    struct media_pad_desc pads_1[] = {
        /* entity, index, flags, reserved[2]*/
        {   1,       0,     5,     {0, 0}},
        {   1,       1,     2,     {0, 0}}
    };

    struct media_link_desc links_1[] = {
        /*     source            sink           flags   reserved[2] */
        {{1, 1, 2, {0, 0}}, {2, 0, 5, {0, 0}},   0,      {0, 0}},
        {{1, 1, 2, {0, 0}}, {15, 0, 5, {0, 0}},  0,      {0, 0}},
        {{1, 1, 2, {0, 0}}, {17, 0, 5, {0, 0}},  0,      {0, 0}},
    };
    setup_link_enum(1, 2, 3);


    struct media_pad_desc pads_2[] = {
        /* entity, index, flags, reserved[2]*/
        {   2,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_2[] = {};
    setup_link_enum(2, 1, 0);


    struct media_pad_desc pads_3[] = {
        /* entity, index, flags, reserved[2]*/
        {   3,       0,     5,     {0, 0}},
        {   3,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_3[] = {
        /*     source            sink           flags   reserved[2] */
        {{3, 1, 2, {0, 0}}, {4, 0, 5,  {0, 0}},  0,       {0, 0}},
        {{3, 1, 2, {0, 0}}, {15, 0, 5, {0, 0}},  0,      {0, 0}},
        {{3, 1, 2, {0, 0}}, {17, 0, 5, {0, 0}},  0,      {0, 0}},
    };
    setup_link_enum(3, 2, 3);


    struct media_pad_desc pads_4[] = {
        /* entity, index, flags, reserved[2]*/
        {   4,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_4[] = {};
    setup_link_enum(4, 1, 0);


    struct media_pad_desc pads_5[] = {
        /* entity, index, flags, reserved[2]*/
        {   5,       0,     5,     {0, 0}},
        {   5,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_5[] = {
        /*     source            sink           flags   reserved[2] */
        {{5, 1, 2, {0, 0}}, {6, 0, 5,  {0, 0}},   0,       {0, 0}},
        {{5, 1, 2, {0, 0}}, {15, 0, 5, {0, 0}},   0,      {0, 0}},
        {{5, 1, 2, {0, 0}}, {17, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(5, 2 , 3);


    struct media_pad_desc pads_6[] = {
        /* entity, index, flags, reserved[2]*/
        {   6,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_6[] = {};
    setup_link_enum(6, 1 , 0);


    struct media_pad_desc pads_7[] = {
        /* entity, index, flags, reserved[2]*/
        {   7,       0,     5,     {0, 0}},
        {   7,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_7[] = {
        /*     source            sink           flags   reserved[2] */
        {{7, 1, 2, {0, 0}}, {8, 0, 5, {0, 0}},    0,       {0, 0}},
        {{7, 1, 2, {0, 0}}, {15, 0, 5, {0, 0}},   0,      {0, 0}},
        {{7, 1, 2, {0, 0}}, {17, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(7, 2, 3);


    struct media_pad_desc pads_8[] = {
        /* entity, index, flags, reserved[2]*/
        {   8,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_8[] = {};
    setup_link_enum(8, 1, 0);


    struct media_pad_desc pads_9[] = {
        /* entity, index, flags, reserved[2]*/
        {   9,       0,     5,     {0, 0}},
        {   9,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_9[] = {
        /*     source            sink           flags   reserved[2] */
        {{9, 1, 2, {0, 0}}, {10, 0, 5, {0, 0}},   0,      {0, 0}},
        {{9, 1, 2, {0, 0}}, {15, 0, 5, {0, 0}},   0,      {0, 0}},
        {{9, 1, 2, {0, 0}}, {17, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(9, 2 , 3);


    struct media_pad_desc pads_10[] = {
        /* entity, index, flags, reserved[2]*/
        {   10,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_10[] = {};
    setup_link_enum(10, 1, 0);


    struct media_pad_desc pads_11[] = {
        /* entity, index, flags, reserved[2]*/
        {   11,       0,     5,     {0, 0}},
        {   11,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_11[] = {
        /*     source            sink           flags   reserved[2] */
        {{11, 1, 2, {0, 0}}, {12, 0, 5, {0, 0}},   0,      {0, 0}},
        {{11, 1, 2, {0, 0}}, {15, 0, 5, {0, 0}},   0,      {0, 0}},
        {{11, 1, 2, {0, 0}}, {17, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(11, 2, 3);


    struct media_pad_desc pads_12[] = {
        /* entity, index, flags, reserved[2]*/
        {   12,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_12[] = {};
    setup_link_enum(12, 1, 0);


    struct media_pad_desc pads_13[] = {
        /* entity, index, flags, reserved[2]*/
        {   13,       0,     2,     {0, 0}},
    };
    struct media_link_desc links_13[] = {
        /*     source            sink           flags   reserved[2] */
        {{13, 0, 2, {0, 0}}, {14, 0, 5, {0, 0}},   0,      {0, 0}},
        {{13, 0, 2, {0, 0}}, {15, 0, 5, {0, 0}},   1,      {0, 0}},
    };
    setup_link_enum(13, 1, 2);


    struct media_pad_desc pads_14[] = {
        /* entity, index, flags, reserved[2]*/
        {   14,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_14[] = {};
    setup_link_enum(14, 1, 0);


    struct media_pad_desc pads_15[] = {
        /* entity, index, flags, reserved[2]*/
        {   15,       0,     5,     {0, 0}},
        {   15,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_15[] = {
        /*     source            sink           flags   reserved[2] */
        {{15, 1, 2, {0, 0}}, {16, 0, 5, {0, 0}},   1,      {0, 0}},
        {{15, 1, 2, {0, 0}}, {19, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(15, 2, 2);


    struct media_pad_desc pads_16[] = {
        /* entity, index, flags, reserved[2]*/
        {   16,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_16[] = {};
    setup_link_enum(16, 1, 0);


    struct media_pad_desc pads_17[] = {
        /* entity, index, flags, reserved[2]*/
        {   17,       0,     5,     {0, 0}},
        {   17,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_17[] = {
        /*     source            sink           flags   reserved[2] */
        {{17, 1, 2, {0, 0}}, {18, 0, 5, {0, 0}},   3,      {0, 0}},
    };
    setup_link_enum(17, 2, 1);


    struct media_pad_desc pads_18[] = {
        /* entity, index, flags, reserved[2]*/
        {   18,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_18[] = {};
    setup_link_enum(18, 1, 0);


    struct media_pad_desc pads_19[] = {
        /* entity, index, flags, reserved[2]*/
        {   19,       0,     5,     {0, 0}},
        {   19,       1,     6,     {0, 0}},
        {   19,       2,     5,     {0, 0}},
        {   19,       3,     2,     {0, 0}},
        {   19,       4,     2,     {0, 0}},
    };
    struct media_link_desc links_19[] = {
        /*     source            sink           flags   reserved[2] */
        {{19, 1, 6, {0, 0}}, {20, 0, 5, {0, 0}},   0,      {0, 0}},
        {{19, 3, 2, {0, 0}}, {22, 0, 5, {0, 0}},   0,      {0, 0}},
        {{19, 4, 2, {0, 0}}, {23, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(19, 5, 3);

    struct media_pad_desc pads_20[] = {
        /* entity, index, flags, reserved[2]*/
        {   20,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_20[] = { };
    setup_link_enum(20, 1 , 0);

    struct media_pad_desc pads_21[] = {
        /* entity, index, flags, reserved[2]*/
        {   21,       0,     6,     {0, 0}},
    };
    struct media_link_desc links_21[] = {
        /*     source            sink           flags   reserved[2] */
        {{21, 0, 6, {0, 0}}, {19, 2, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(21, 1, 1);

    struct media_pad_desc pads_22[] = {
        /* entity, index, flags, reserved[2]*/
        {   22,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_22[] = { };
    setup_link_enum(22, 1, 0);


    struct media_pad_desc pads_23[] = {
        /* entity, index, flags, reserved[2]*/
        {   23,       0,     5,     {0, 0}},
    };
    struct media_link_desc links_23[] = {};
    setup_link_enum(23, 1, 0);


    struct media_pad_desc pads_24[] = {
        /* entity, index, flags, reserved[2]*/
        {   24,       0,     2,     {0, 0}},
    };
    struct media_link_desc links_24[] = {
        /*     source            sink           flags   reserved[2] */
        {{24, 0, 2, {0, 0}}, {25, 0, 1, {0, 0}},   3,      {0, 0}},
    };
    setup_link_enum(24, 1, 1);

    struct media_pad_desc pads_25[] = {
        /* entity, index, flags, reserved[2]*/
        {   25,       0,     1,     {0, 0}},
        {   25,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_25[] = {
        /*     source            sink           flags   reserved[2] */
        {{25, 1, 2, {0, 0}}, {1, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(25, 2, 1);

    struct media_pad_desc pads_26[] = {
        /* entity, index, flags, reserved[2]*/
        {   26,       0,     2,     {0, 0}},
    };
    struct media_link_desc links_26[] = {
        /*     source            sink           flags   reserved[2] */
        {{26, 0, 2, {0, 0}}, {27, 0, 1, {0, 0}},   3,      {0, 0}},
    };
    setup_link_enum(26, 1, 1);

    struct media_pad_desc pads_27[] = {
        /* entity, index, flags, reserved[2]*/
        {   27,       0,     1,     {0, 0}},
        {   27,       1,     2,     {0, 0}},
    };
    struct media_link_desc links_27[] = {
        /*     source            sink           flags   reserved[2] */
        {{27, 1, 2, {0, 0}}, {1, 0, 5, {0, 0}},   0,      {0, 0}},
    };
    setup_link_enum(27, 2, 1);

    EXPECT_CALL(*this,  ioctl(MEDIACTL_FD, MEDIA_IOC_ENUM_LINKS, A<struct media_links_enum*>()))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[0]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[1]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[2]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[3]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[4]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[5]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[6]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[7]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[8]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[9]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[10]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[11]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[12]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[13]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[14]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[15]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[16]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[17]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[18]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[19]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[20]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[21]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[22]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[23]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[24]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[25]), Return(0)))
        .WillOnce(DoAll(SetArgPointee<2>(mLinkEnum[26]), Return(0)))
        .WillRepeatedly(Return(0));

    return 0;
}

int MockSysCall::mockMediaCtlIoctl_MEDIA_IOC_SETUP_LINK()
{
    //ON_CALL(*this,  ioctl(_, MEDIA_IOC_SETUP_LINK, A<struct media_link_desc*>()))
    //        .WillByDefault(Return(0));

    EXPECT_CALL(*this,  ioctl(MEDIACTL_FD, MEDIA_IOC_SETUP_LINK, A<struct media_link_desc*>()))
        .WillRepeatedly(Return(0));

    return 0;
}

int MockSysCall::mockMediaCtlIoctl()
{

    mockMediaCtlIoctl_MEDIA_IOC_DEVICE_INFO();
    mockMediaCtlIoctl_MEDIA_IOC_ENUM_ENTITIES();
    mockMediaCtlIoctl_MEDIA_IOC_ENUM_LINKS();
    mockMediaCtlIoctl_MEDIA_IOC_SETUP_LINK();
    return 0;
}

int MockSysCall::mockV4l2DevIoctl_VIDIOC_QUERYCAP()
{
    struct v4l2_capability cap = {
        "mockV4l2Driver",
        "mockCard",
        "mockBus",
        1,
        V4L2_CAP_VIDEO_OUTPUT | V4L2_CAP_STREAMING | V4L2_CAP_VIDEO_CAPTURE,
        0,
        0
    };

    EXPECT_CALL(*this,  ioctl(V4L2DEV_FD, VIDIOC_QUERYCAP, A<struct v4l2_capability *>()))
        .WillRepeatedly(DoAll(SetArgPointee<2>(cap), Return(0)));
    return 0;
}

int MockSysCall::mockV4l2DevIoctl_VIDIOC_ENUM_FMT()
{
    return 0;
}

int MockSysCall::mockV4l2DevIoctl_VIDIOC_EXPBUF()
{
    struct v4l2_exportbuffer expbuf = {0, 0, 0, 0, 2};

    EXPECT_CALL(*this,  ioctl(V4L2DEV_FD, VIDIOC_EXPBUF, A<struct v4l2_exportbuffer *>()))
        .WillRepeatedly(DoAll(SetArgPointee<2>(expbuf), Return(0)));
    return 0;
}

int MockSysCall::mockV4l2SubDevIoctl_VIDIOC_S_CTRL()
{
    ON_CALL(*this,	ioctl(V4L2SUBDEV_FD, VIDIOC_S_CTRL, A<struct v4l2_control *>())).WillByDefault(Return(0));
    return 0;

}

int MockSysCall::mockV4l2SubDevIoctl_VIDIOC_QUERYCTRL()
{
    ON_CALL(*this,	ioctl(V4L2SUBDEV_FD, VIDIOC_QUERYCTRL, A<struct v4l2_queryctrl *>())).WillByDefault(Return(0));
    return 0;

}

int MockSysCall::mockV4l2SubDevIoctl_VIDIOC_SUBDEV_S_SELECTION()
{
    ON_CALL(*this,	ioctl(V4L2SUBDEV_FD, VIDIOC_SUBDEV_S_SELECTION, A<struct v4l2_subdev_selection *>())).WillByDefault(Return(0));
    return 0;

}


int MockSysCall::mockV4l2DevIoctl()
{
    mockV4l2DevIoctl_VIDIOC_QUERYCAP();
    mockV4l2DevIoctl_VIDIOC_ENUM_FMT();
    mockV4l2DevIoctl_VIDIOC_EXPBUF();
    return 0;
}

int MockSysCall::mockV4l2SubDevIoctl()
{
    mockV4l2SubDevIoctl_VIDIOC_S_CTRL();
    mockV4l2SubDevIoctl_VIDIOC_QUERYCTRL();
    mockV4l2SubDevIoctl_VIDIOC_SUBDEV_S_SELECTION();

    return 0;
}

int MockSysCall::ioctl(int fd, int request, struct v4l2_format *vformat)
{
    LOGD("Set format for FD %d", fd);

    int format = vformat->fmt.pix.pixelformat;
    int width = vformat->fmt.pix.width;
    int height = vformat->fmt.pix.height;
    int bufferSize = CameraUtils::getFrameSize(format, width, height);

    vformat->fmt.pix.sizeimage = bufferSize;
    memcpy(&mV4l2DevFmt, vformat, sizeof(v4l2_format));
    LOGD("Mock VIDIOC_S_FMT type %d : resolution:(%dx%d), bpl: %d, format: %s, field: %d",
            mV4l2DevFmt.type,
            mV4l2DevFmt.fmt.pix.width,
            mV4l2DevFmt.fmt.pix.height,
            mV4l2DevFmt.fmt.pix.bytesperline,
            CameraUtils::pixelCode2String(mV4l2DevFmt.fmt.pix.pixelformat),
            mV4l2DevFmt.fmt.pix.field);

    return 0;
}

int MockSysCall::poll(struct pollfd *pfd, nfds_t nfds, int timeout)
{
    LOG2("%s: Mock poll device", __func__);
    if (pfd != NULL) {
        pfd[0].revents = POLLPRI | POLLIN;
        return 1;
    } else {
        LOGE("%s: Input fd is NULL", __func__);
        return -1;
    }
}

int MockSysCall::ioctl(int fd, int request, struct v4l2_buffer *arg)
{
    LOG2("Buffer IOCTL: handle: %d, request: %d, buffer: %lu (%d:%d)", fd, request, arg->m.userptr, (int)VIDIOC_QBUF, (int)VIDIOC_DQBUF);
    if (request == (int)VIDIOC_QBUF) {
        LOG2("Enqueue buffer index %d, addr: %lu, length: %d", arg->index, arg->m.userptr, arg->length);
    } else if (request == (int)VIDIOC_DQBUF) {
        usleep(FPS_200_INTERVAL);
        readFileImgIntoBuf((char*)arg->m.userptr, arg->length, arg->index);
        LOG2("Dequeue buffer: handle: %d, request: %d, buffer: %lu", fd, request, arg->m.userptr);

    }
    return 0;
}

int MockSysCall::ioctl(int fd, int request, struct v4l2_event *arg)
{
    arg->sequence++;
    return 0;
}

void *MockSysCall::mmap(void *addr, size_t len, int prot, int flag, int filedes, off_t off)
{
    void *buf = NULL;
    int ret = posix_memalign(&buf, getpagesize(), len);
    if (ret != 0) {
        LOGD("%s, failed to allocate mmap buffer\n", __func__);
        return NULL;
    }
    return buf;
}

int MockSysCall::munmap(void *addr, size_t len)
{
    free(addr);
    return 0;
}

int MockSysCall::readFileImgIntoBuf(char* destBuf, int bufSize, int index)
{
    //data file which store images data.
    char imgsDataFile[FILE_NAME_LENGTH];
    //info file which store information, like image size, for related data file.
    char imgsInfoFile[FILE_NAME_LENGTH];

    memset(imgsDataFile, 0, FILE_NAME_LENGTH);
    snprintf(imgsDataFile, FILE_NAME_LENGTH, "imgs_%d_%d_%d.data", mV4l2DevFmt.fmt.pix.width, mV4l2DevFmt.fmt.pix.height, mV4l2DevFmt.fmt.pix.pixelformat);

    memset(imgsInfoFile, 0, FILE_NAME_LENGTH);
    snprintf(imgsInfoFile, FILE_NAME_LENGTH, "imgs_%d_%d_%d.info", mV4l2DevFmt.fmt.pix.width, mV4l2DevFmt.fmt.pix.height, mV4l2DevFmt.fmt.pix.pixelformat);

    FILE* infoHandle = fopen(imgsInfoFile, "rt");
    if (!infoHandle) {
        LOG2("Warning: could not open images data file %s (imgs_[width]_[height]_[fourccformat].info), skip test image data copy...", imgsDataFile);
        return -1;
    }

    int imgSize = 0;
    if (fscanf(infoHandle, "%d", &imgSize) < 0){
        LOG2("Warning: failed to read image size");
        fclose(infoHandle);
        return -1;
    }

    if  (imgSize > bufSize) {
        LOG2("Warning: image size from info file %s is larger than buffer size", imgsInfoFile);
        fclose(infoHandle);
        return -1;
    }

    fclose(infoHandle);

    FILE* fHandle = fopen(imgsDataFile, "rb");
    if (!fHandle) {
        LOG2("Warning: could not open images data file %s (imgs_[width]_[height]_[fourccformat].data), skip test image data copy...", imgsDataFile);
        return -1;
    }

    fseek(fHandle, 0, SEEK_END);
    int fsize = ftell(fHandle);
    fseek(fHandle, 0, SEEK_SET);

    //index is queue buffer index, we need to calculate the image data index here.
    int imgCntInDataFile = (int)(fsize / imgSize);
    if (index > imgCntInDataFile) {
        mImgDataIndex = index % imgCntInDataFile;
    } else {
        mImgDataIndex ++;
        if  (mImgDataIndex >= imgCntInDataFile)
            mImgDataIndex = 0;
    }

    LOG2("Image data file total size: %d, data file index %d, data file offset %d, current image size %d", fsize, mImgDataIndex, mImgDataIndex * imgSize, imgSize);

    if (fseek(fHandle, mImgDataIndex * imgSize, SEEK_SET) != 0) {
        LOG2("Error: cound not locate the No. %d image", mImgDataIndex);
        fclose(fHandle);
        return -1;
    }

    int bytes = fread(destBuf, sizeof(char), imgSize, fHandle);

    if (bytes < 0) {
        LOG2("Error: failed to read image file");
        fclose(fHandle);
        return -1;
    }

    fclose(fHandle);
    LOG2("Test image copy done!");

    return 0;


}


