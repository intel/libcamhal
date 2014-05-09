/*
 * Copyright (C) 2012 The Android Open Source Project
 * Copyright (C) 2015-2018 Intel Corporation
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

#define IF_ALOGV() if (false)

#define LOG_TAG "camera_metadata_tests"

#include <errno.h>
#include <vector>
#include <algorithm>

#include "gtest/gtest.h"
#include "icamera_metadata_base.h"
#include "case_common.h"
#include "Utils.h"

#define OK    0
#define ERROR 1
#define NOT_FOUND (-ENOENT)

#define _Alignas(T) \
    ({struct _AlignasStruct { char c; T field; };       \
        offsetof(struct _AlignasStruct, field); })

#define FINISH_USING_CAMERA_METADATA(m)                         \
    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL)); \
    free_icamera_metadata(m);                                    \

TEST(icamera_metadata, allocate_normal) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    EXPECT_NOT_NULL(m);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, allocate_clone_normal) {
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;
    icamera_metadata_t *src = NULL;
    icamera_metadata_t *copy = NULL;

    src = allocate_icamera_metadata(entry_capacity, data_capacity);
    size_t memory_needed = calculate_icamera_metadata_size(entry_capacity,
                                                              data_capacity);

    copy = allocate_copy_icamera_metadata_checked(NULL, memory_needed);
    EXPECT_EQ((void*)NULL, (void*)copy);

    ASSERT_NE((void*)NULL, (void*)src);
    copy = allocate_copy_icamera_metadata_checked(src, memory_needed);
    ASSERT_NE((void*)NULL, (void*)copy);

    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(copy));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(copy));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(copy));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(copy));

    FINISH_USING_CAMERA_METADATA(src);
    FINISH_USING_CAMERA_METADATA(copy);
}

TEST(icamera_metadata, allocate_nodata) {
    icamera_metadata_t *m = NULL;

    m = allocate_icamera_metadata(1, 0);

    EXPECT_NOT_NULL(m);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(m));
    EXPECT_EQ((size_t)1, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_capacity(m));

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, clone_nodata) {
    icamera_metadata_t *src = NULL;
    icamera_metadata_t *copy = NULL;

    src = allocate_icamera_metadata(10, 0);

    ASSERT_NE((void*)NULL, (void*)src);
    copy = clone_icamera_metadata(src);
    ASSERT_NE((void*)NULL, (void*)copy);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(copy));
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_capacity(copy));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(copy));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_capacity(copy));

    FINISH_USING_CAMERA_METADATA(src);
    FINISH_USING_CAMERA_METADATA(copy);
}

TEST(icamera_metadata, allocate_nothing) {
    icamera_metadata_t *m = NULL;

    m = allocate_icamera_metadata(0, 0);

    ASSERT_NE((void*)NULL, (void*)m);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_capacity(m));
}

TEST(icamera_metadata, place_normal) {
    icamera_metadata_t *m = NULL;
    void *buf = NULL;

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    size_t buf_size = calculate_icamera_metadata_size(entry_capacity,
            data_capacity);

    EXPECT_TRUE(buf_size > 0);

    buf = malloc(buf_size);

    EXPECT_NOT_NULL(buf);

    m = place_icamera_metadata(buf, buf_size, entry_capacity, data_capacity);

    EXPECT_EQ(buf, (uint8_t*)m);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, &buf_size));

    free(buf);
}

TEST(icamera_metadata, place_nospace) {
    icamera_metadata_t *m = NULL;
    void *buf = NULL;

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    size_t buf_size = calculate_icamera_metadata_size(entry_capacity,
            data_capacity);

    EXPECT_GT(buf_size, (size_t)0);

    buf_size--;

    buf = malloc(buf_size);

    EXPECT_NOT_NULL(buf);

    m = place_icamera_metadata(buf, buf_size, entry_capacity, data_capacity);

    EXPECT_NULL(m);

    free(buf);
}

TEST(icamera_metadata, place_extraspace) {
    icamera_metadata_t *m = NULL;
    uint8_t *buf = NULL;

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;
    const size_t extra_space = 10;

    size_t buf_size = calculate_icamera_metadata_size(entry_capacity,
            data_capacity);

    EXPECT_GT(buf_size, (size_t)0);

    buf_size += extra_space;

    buf = (uint8_t*)malloc(buf_size);

    EXPECT_NOT_NULL(buf);

    m = place_icamera_metadata(buf, buf_size, entry_capacity, data_capacity);

    EXPECT_EQ((uint8_t*)m, buf);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));
    EXPECT_EQ(buf + buf_size - extra_space, (uint8_t*)m + get_icamera_metadata_size(m));

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, &buf_size));

    free(buf);
}

TEST(icamera_metadata, get_size) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    EXPECT_EQ(calculate_icamera_metadata_size(entry_capacity, data_capacity),
            get_icamera_metadata_size(m) );

    EXPECT_EQ(calculate_icamera_metadata_size(0,0),
            get_icamera_metadata_compact_size(m) );

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, add_get_normal) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 128;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL));

    int result;
    size_t data_used = 0;
    size_t entries_used = 0;

    // INT64

    int64_t exposure_time = 1000000000;
    result = add_icamera_metadata_entry(m,
            CAMERA_SENSOR_EXPOSURE_TIME,
            &exposure_time, 1);
    EXPECT_EQ(OK, result);
    data_used += calculate_icamera_metadata_entry_data_size(
            get_icamera_metadata_tag_type(CAMERA_SENSOR_EXPOSURE_TIME), 1);
    entries_used++;

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL));

    // INT32

    int32_t sensitivity = 800;
    result = add_icamera_metadata_entry(m,
            CAMERA_SENSOR_SENSITIVITY,
            &sensitivity, 1);
    EXPECT_EQ(OK, result);
    data_used += calculate_icamera_metadata_entry_data_size(
            get_icamera_metadata_tag_type(CAMERA_SENSOR_SENSITIVITY), 1);
    entries_used++;

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL));

    // FLOAT

    float focusDistance = 0.5f;
    result = add_icamera_metadata_entry(m,
            CAMERA_LENS_FOCUS_DISTANCE,
            &focusDistance, 1);
    EXPECT_EQ(OK, result);
    data_used += calculate_icamera_metadata_entry_data_size(
            get_icamera_metadata_tag_type(CAMERA_LENS_FOCUS_DISTANCE), 1);
    entries_used++;

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL));

    // Array of FLOAT

    float colorCorrectionGains[] = {1.69f,  1.00f,  1.00f,  2.41f};
    result = add_icamera_metadata_entry(m,
            CAMERA_AWB_COLOR_GAINS,
            colorCorrectionGains, ARRAY_SIZE(colorCorrectionGains));
    EXPECT_EQ(OK, result);
    data_used += calculate_icamera_metadata_entry_data_size(
           get_icamera_metadata_tag_type(CAMERA_AWB_COLOR_GAINS),
           ARRAY_SIZE(colorCorrectionGains));
    entries_used++;

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL));


    // Array of RATIONAL

    float colorTransform[] = {
        0.9, 0,   0,
        0.2, 0.5, 0,
        0,   0.1, 0.7
    };
    result = add_icamera_metadata_entry(m,
            CAMERA_AWB_COLOR_TRANSFORM,
            colorTransform, ARRAY_SIZE(colorTransform));
    EXPECT_EQ(OK, result);
    data_used += calculate_icamera_metadata_entry_data_size(
           get_icamera_metadata_tag_type(CAMERA_AWB_COLOR_TRANSFORM),
           ARRAY_SIZE(colorTransform));
    entries_used++;

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m, NULL));

    // Check added entries

    size_t index = 0;
    icamera_metadata_entry entry;

    result = get_icamera_metadata_entry(m,
            index, &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(index, (int)entry.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, entry.type);
    EXPECT_EQ((size_t)1, entry.count);
    EXPECT_EQ(exposure_time, *entry.data.i64);
    index++;

    result = get_icamera_metadata_entry(m,
            index, &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(index, entry.index);
    EXPECT_EQ(CAMERA_SENSOR_SENSITIVITY, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, entry.type);
    EXPECT_EQ((size_t)1, entry.count);
    EXPECT_EQ(sensitivity, *entry.data.i32);
    index++;

    result = get_icamera_metadata_entry(m,
            index, &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(index, entry.index);
    EXPECT_EQ(CAMERA_LENS_FOCUS_DISTANCE, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ((size_t)1, entry.count);
    EXPECT_EQ(focusDistance, *entry.data.f);
    index++;

    result = get_icamera_metadata_entry(m,
            index, &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(index, entry.index);
    EXPECT_EQ(CAMERA_AWB_COLOR_GAINS, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ(ARRAY_SIZE(colorCorrectionGains), entry.count);
    for (unsigned int i=0; i < entry.count; i++) {
        EXPECT_EQ(colorCorrectionGains[i], entry.data.f[i]);
    }
    index++;

    result = get_icamera_metadata_entry(m,
            index, &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(index, entry.index);
    EXPECT_EQ(CAMERA_AWB_COLOR_TRANSFORM, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ(ARRAY_SIZE(colorTransform), entry.count);
    for (unsigned int i=0; i < entry.count; i++) {
        EXPECT_EQ(colorTransform[i], entry.data.f[i]);
    }
    index++;

    EXPECT_EQ(calculate_icamera_metadata_size(entry_capacity, data_capacity),
            get_icamera_metadata_size(m) );

    EXPECT_EQ(calculate_icamera_metadata_size(entries_used, data_used),
            get_icamera_metadata_compact_size(m) );

    IF_ALOGV() {
        dump_icamera_metadata(m, 0, 2);
    }

    FINISH_USING_CAMERA_METADATA(m);
}

void add_test_metadata(icamera_metadata_t *m, int entry_count) {

    EXPECT_NOT_NULL(m);

    size_t data_used = 0;
    size_t entries_used = 0;
    int64_t exposure_time;
    for (int i=0; i < entry_count; i++ ) {
        exposure_time = 100 + i * 100;
        int result = add_icamera_metadata_entry(m,
                CAMERA_SENSOR_EXPOSURE_TIME,
                &exposure_time, 1);
        EXPECT_EQ(OK, result);
        data_used += calculate_icamera_metadata_entry_data_size(
                get_icamera_metadata_tag_type(CAMERA_SENSOR_EXPOSURE_TIME), 1);
        entries_used++;
    }
    EXPECT_EQ(data_used, get_icamera_metadata_data_count(m));
    EXPECT_EQ(entries_used, get_icamera_metadata_entry_count(m));
    EXPECT_GE(get_icamera_metadata_data_capacity(m),
            get_icamera_metadata_data_count(m));
}

TEST(icamera_metadata, add_get_toomany) {
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;
    icamera_metadata_t* m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    int32_t sensitivity = 100;
    int result = add_icamera_metadata_entry(m, CAMERA_SENSOR_SENSITIVITY, &sensitivity, 1);

    EXPECT_EQ(ERROR, result);

    icamera_metadata_entry entry;
    for (unsigned int i=0; i < entry_capacity; i++) {
        int64_t exposure_time = 100 + i * 100;
        result = get_icamera_metadata_entry(m,
                i, &entry);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, entry.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, entry.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, entry.type);
        EXPECT_EQ((size_t)1, entry.count);
        EXPECT_EQ(exposure_time, *entry.data.i64);
    }
    entry.tag = 1234;
    entry.type = 56;
    entry.data.u8 = NULL;
    entry.count = 7890;
    result = get_icamera_metadata_entry(m,
            entry_capacity, &entry);
    EXPECT_EQ(ERROR, result);
    EXPECT_EQ((uint32_t)1234, entry.tag);
    EXPECT_EQ((uint8_t)56, entry.type);
    EXPECT_EQ(NULL, entry.data.u8);
    EXPECT_EQ((size_t)7890, entry.count);

    IF_ALOGV() {
        dump_icamera_metadata(m, 0, 2);
    }

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, add_too_much_data) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    int result;
    size_t data_used = entry_capacity * calculate_icamera_metadata_entry_data_size(
        get_icamera_metadata_tag_type(CAMERA_SENSOR_EXPOSURE_TIME), 1);
    m = allocate_icamera_metadata(entry_capacity + 1, data_used);


    add_test_metadata(m, entry_capacity);

    int64_t exposure_time = 12345;
    result = add_icamera_metadata_entry(m,
            CAMERA_SENSOR_EXPOSURE_TIME,
            &exposure_time, 1);
    EXPECT_EQ(ERROR, result);

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, copy_metadata) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 50;
    const size_t data_capacity = 450;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    size_t buf_size = get_icamera_metadata_compact_size(m);
    EXPECT_LT((size_t)0, buf_size);

    uint8_t *buf = (uint8_t*)malloc(buf_size);
    EXPECT_NOT_NULL(buf);

    icamera_metadata_t *m2 = copy_icamera_metadata(buf, buf_size, m);
    EXPECT_NOT_NULL(m2);
    EXPECT_EQ(buf, (uint8_t*)m2);
    EXPECT_EQ(get_icamera_metadata_entry_count(m),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(get_icamera_metadata_entry_capacity(m2),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_capacity(m2),
            get_icamera_metadata_data_count(m2));

    for (unsigned int i=0; i < get_icamera_metadata_entry_count(m); i++) {
        icamera_metadata_entry e1, e2;
        int result;
        result = get_icamera_metadata_entry(m, i, &e1);
        EXPECT_EQ(OK, result);
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(e1.index, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        EXPECT_EQ(e1.type, e2.type);
        EXPECT_EQ(e1.count, e2.count);
        for (unsigned int j=0;
             j < e1.count * icamera_metadata_type_size[e1.type];
             j++) {
            EXPECT_EQ(e1.data.u8[j], e2.data.u8[j]);
        }
    }

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m2, &buf_size));
    free(buf);

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, copy_metadata_extraspace) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 12;
    const size_t data_capacity = 100;

    const size_t extra_space = 10;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    size_t buf_size = get_icamera_metadata_compact_size(m);
    EXPECT_LT((size_t)0, buf_size);
    buf_size += extra_space;

    uint8_t *buf = (uint8_t*)malloc(buf_size);
    EXPECT_NOT_NULL(buf);

    icamera_metadata_t *m2 = copy_icamera_metadata(buf, buf_size, m);
    EXPECT_NOT_NULL(m2);
    EXPECT_EQ(buf, (uint8_t*)m2);
    EXPECT_EQ(get_icamera_metadata_entry_count(m),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(get_icamera_metadata_entry_capacity(m2),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_capacity(m2),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(buf + buf_size - extra_space,
            (uint8_t*)m2 + get_icamera_metadata_size(m2) );

    for (unsigned int i=0; i < get_icamera_metadata_entry_count(m); i++) {
        icamera_metadata_entry e1, e2;

        int result;
        result = get_icamera_metadata_entry(m, i, &e1);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e1.index);
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(e1.index, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        EXPECT_EQ(e1.type, e2.type);
        EXPECT_EQ(e1.count, e2.count);
        for (unsigned int j=0;
             j < e1.count * icamera_metadata_type_size[e1.type];
             j++) {
            EXPECT_EQ(e1.data.u8[j], e2.data.u8[j]);
        }
    }

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m2, &buf_size));
    free(buf);

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, copy_metadata_nospace) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    size_t buf_size = get_icamera_metadata_compact_size(m);
    EXPECT_LT((size_t)0, buf_size);

    buf_size--;

    uint8_t *buf = (uint8_t*)malloc(buf_size);
    EXPECT_NOT_NULL(buf);

    icamera_metadata_t *m2 = copy_icamera_metadata(buf, buf_size, m);
    EXPECT_NULL(m2);

    free(buf);

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, append_metadata) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    icamera_metadata_t *m2 = NULL;

    m2 = allocate_icamera_metadata(entry_capacity*2, data_capacity*2);
    EXPECT_NOT_NULL(m2);

    result = append_icamera_metadata(m2, m);

    EXPECT_EQ(OK, result);

    EXPECT_EQ(get_icamera_metadata_entry_count(m),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity*2, get_icamera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity*2,  get_icamera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_icamera_metadata_entry_count(m); i++) {
        icamera_metadata_entry e1, e2;
        int result;
        result = get_icamera_metadata_entry(m, i, &e1);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e1.index);
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(e1.index, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        EXPECT_EQ(e1.type, e2.type);
        EXPECT_EQ(e1.count, e2.count);
        for (unsigned int j=0;
             j < e1.count * icamera_metadata_type_size[e1.type];
             j++) {
            EXPECT_EQ(e1.data.u8[j], e2.data.u8[j]);
        }
    }

    result = append_icamera_metadata(m2, m);

    EXPECT_EQ(OK, result);

    EXPECT_EQ(get_icamera_metadata_entry_count(m)*2,
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m)*2,
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity*2, get_icamera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity*2,  get_icamera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_icamera_metadata_entry_count(m2); i++) {
        icamera_metadata_entry e1, e2;

        int result;
        result = get_icamera_metadata_entry(m,
                i % entry_capacity, &e1);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i % entry_capacity, e1.index);
        result = get_icamera_metadata_entry(m2,
                i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        EXPECT_EQ(e1.type, e2.type);
        EXPECT_EQ(e1.count, e2.count);
        for (unsigned int j=0;
             j < e1.count * icamera_metadata_type_size[e1.type];
             j++) {
            EXPECT_EQ(e1.data.u8[j], e2.data.u8[j]);
        }
    }

    FINISH_USING_CAMERA_METADATA(m);
    FINISH_USING_CAMERA_METADATA(m2);
}

TEST(icamera_metadata, append_metadata_nospace) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    icamera_metadata_t *m2 = NULL;

    m2 = allocate_icamera_metadata(entry_capacity-1, data_capacity);
    EXPECT_NOT_NULL(m2);

    result = append_icamera_metadata(m2, m);

    EXPECT_EQ(ERROR, result);
    EXPECT_EQ((size_t)0, get_icamera_metadata_entry_count(m2));
    EXPECT_EQ((size_t)0, get_icamera_metadata_data_count(m2));

    FINISH_USING_CAMERA_METADATA(m);
    FINISH_USING_CAMERA_METADATA(m2);
}

TEST(icamera_metadata, append_metadata_onespace) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 50;
    const size_t entry_capacity2 = entry_capacity * 2 - 2;
    const size_t data_capacity2 = data_capacity * 2;
    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, entry_capacity);

    icamera_metadata_t *m2 = NULL;

    m2 = allocate_icamera_metadata(entry_capacity2, data_capacity2);
    EXPECT_NOT_NULL(m2);

    result = append_icamera_metadata(m2, m);

    EXPECT_EQ(OK, result);

    EXPECT_EQ(get_icamera_metadata_entry_count(m),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity2, get_icamera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity2,  get_icamera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_icamera_metadata_entry_count(m); i++) {
        icamera_metadata_entry e1, e2;

        int result;
        result = get_icamera_metadata_entry(m, i, &e1);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e1.index);
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(e1.index, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        EXPECT_EQ(e1.type, e2.type);
        EXPECT_EQ(e1.count, e2.count);
        for (unsigned int j=0;
             j < e1.count * icamera_metadata_type_size[e1.type];
             j++) {
            EXPECT_EQ(e1.data.u8[j], e2.data.u8[j]);
        }
    }

    result = append_icamera_metadata(m2, m);

    EXPECT_EQ(ERROR, result);
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(entry_capacity2, get_icamera_metadata_entry_capacity(m2));
    EXPECT_EQ(data_capacity2,  get_icamera_metadata_data_capacity(m2));

    for (unsigned int i=0; i < get_icamera_metadata_entry_count(m2); i++) {
        icamera_metadata_entry e1, e2;

        int result;
        result = get_icamera_metadata_entry(m,
                i % entry_capacity, &e1);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i % entry_capacity, e1.index);
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        EXPECT_EQ(e1.type, e2.type);
        EXPECT_EQ(e1.count, e2.count);
        for (unsigned int j=0;
             j < e1.count * icamera_metadata_type_size[e1.type];
             j++) {
            EXPECT_EQ(e1.data.u8[j], e2.data.u8[j]);
        }
    }

    FINISH_USING_CAMERA_METADATA(m);
    FINISH_USING_CAMERA_METADATA(m2);
}


TEST(icamera_metadata, add_all_tags) {
    int total_tag_count = 0;
    for (int i = 0; i < CAMERA_SECTION_COUNT; i++) {
        total_tag_count += icamera_metadata_section_bounds[i][1] -
                icamera_metadata_section_bounds[i][0];
    }
    int entry_data_count = 3;
    int conservative_data_space = total_tag_count * entry_data_count * 8;
    uint8_t data[entry_data_count * 8];
    int32_t *data_int32 = (int32_t *)data;
    float *data_float   = (float *)data;
    int64_t *data_int64 = (int64_t *)data;
    double *data_double = (double *)data;
    icamera_metadata_rational_t *data_rational =
            (icamera_metadata_rational_t *)data;

    icamera_metadata_t *m = allocate_icamera_metadata(total_tag_count,
            conservative_data_space);

    ASSERT_NE((void*)NULL, (void*)m);

    int result;

    int counter = 0;
    for (int i = 0; i < CAMERA_SECTION_COUNT; i++) {
        for (uint32_t tag = icamera_metadata_section_bounds[i][0];
                tag < icamera_metadata_section_bounds[i][1];
             tag++, counter++) {
            int type = get_icamera_metadata_tag_type(tag);
            ASSERT_NE(-1, type);

            switch (type) {
                case ICAMERA_TYPE_BYTE:
                    data[0] = tag & 0xFF;
                    data[1] = (tag >> 8) & 0xFF;
                    data[2] = (tag >> 16) & 0xFF;
                    break;
                case ICAMERA_TYPE_INT32:
                    data_int32[0] = tag;
                    data_int32[1] = i;
                    data_int32[2] = counter;
                    break;
                case ICAMERA_TYPE_FLOAT:
                    data_float[0] = tag;
                    data_float[1] = i;
                    data_float[2] = counter / (float)total_tag_count;
                    break;
                case ICAMERA_TYPE_INT64:
                    data_int64[0] = (int64_t)tag | ( (int64_t)tag << 32);
                    data_int64[1] = i;
                    data_int64[2] = counter;
                    break;
                case ICAMERA_TYPE_DOUBLE:
                    data_double[0] = tag;
                    data_double[1] = i;
                    data_double[2] = counter / (double)total_tag_count;
                    break;
                case ICAMERA_TYPE_RATIONAL:
                    data_rational[0].numerator = tag;
                    data_rational[0].denominator = 1;
                    data_rational[1].numerator = i;
                    data_rational[1].denominator = 1;
                    data_rational[2].numerator = counter;
                    data_rational[2].denominator = total_tag_count;
                    break;
                default:
                    FAIL() << "Unknown type field encountered:" << type;
                    break;
            }
            result = add_icamera_metadata_entry(m,
                    tag,
                    data,
                    entry_data_count);
            ASSERT_EQ(OK, result);

        }
    }

    IF_ALOGV() {
        dump_icamera_metadata(m, 0, 2);
    }

    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, sort_metadata) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 5;
    const size_t data_capacity = 100;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    // Add several unique entries in non-sorted order

    float colorTransform[] = {
        0.9, 0,   0,
        0.2, 0.5, 0,
        0,   0.1, 0.7
    };
    result = add_icamera_metadata_entry(m,
            CAMERA_AWB_COLOR_TRANSFORM,
            colorTransform, ARRAY_SIZE(colorTransform));
    EXPECT_EQ(OK, result);

    float focus_distance = 0.5f;
    result = add_icamera_metadata_entry(m,
            CAMERA_LENS_FOCUS_DISTANCE,
            &focus_distance, 1);
    EXPECT_EQ(OK, result);

    int64_t exposure_time = 1000000000;
    result = add_icamera_metadata_entry(m,
            CAMERA_SENSOR_EXPOSURE_TIME,
            &exposure_time, 1);
    EXPECT_EQ(OK, result);

    int32_t sensitivity = 800;
    result = add_icamera_metadata_entry(m,
            CAMERA_SENSOR_SENSITIVITY,
            &sensitivity, 1);
    EXPECT_EQ(OK, result);

    // Test unsorted find
    icamera_metadata_entry_t entry;
    result = find_icamera_metadata_entry(m,
            CAMERA_LENS_FOCUS_DISTANCE,
            &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(CAMERA_LENS_FOCUS_DISTANCE, entry.tag);
    EXPECT_EQ((size_t)1, entry.index);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ((size_t)1, entry.count);
    EXPECT_EQ(focus_distance, *entry.data.f);

    result = find_icamera_metadata_entry(m,
            CAMERA_NOISE_REDUCTION_STRENGTH,
            &entry);
    EXPECT_EQ(NOT_FOUND, result);
    EXPECT_EQ((size_t)1, entry.index);
    EXPECT_EQ(CAMERA_LENS_FOCUS_DISTANCE, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ((size_t)1, entry.count);
    EXPECT_EQ(focus_distance, *entry.data.f);

    // Sort
    IF_ALOGV() {
        std::cout << "Pre-sorted metadata" << std::endl;
        dump_icamera_metadata(m, 0, 2);
    }

    result = sort_icamera_metadata(m);
    EXPECT_EQ(OK, result);

    IF_ALOGV() {
        std::cout << "Sorted metadata" << std::endl;
        dump_icamera_metadata(m, 0, 2);
    }

    // Test sorted find
    size_t lensFocusIndex = -1;
    {
        std::vector<uint32_t> tags;
        tags.push_back(CAMERA_AWB_COLOR_TRANSFORM);
        tags.push_back(CAMERA_LENS_FOCUS_DISTANCE);
        tags.push_back(CAMERA_SENSOR_EXPOSURE_TIME);
        tags.push_back(CAMERA_SENSOR_SENSITIVITY);
        std::sort(tags.begin(), tags.end());

        lensFocusIndex =
            std::find(tags.begin(), tags.end(), CAMERA_LENS_FOCUS_DISTANCE)
            - tags.begin();
    }

    result = find_icamera_metadata_entry(m,
            CAMERA_LENS_FOCUS_DISTANCE,
            &entry);
    EXPECT_EQ(OK, result);
    EXPECT_EQ(lensFocusIndex, entry.index);
    EXPECT_EQ(CAMERA_LENS_FOCUS_DISTANCE, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ((size_t)1, (size_t)entry.count);
    EXPECT_EQ(focus_distance, *entry.data.f);

    result = find_icamera_metadata_entry(m,
            CAMERA_NOISE_REDUCTION_STRENGTH,
            &entry);
    EXPECT_EQ(NOT_FOUND, result);
    EXPECT_EQ(lensFocusIndex, entry.index);
    EXPECT_EQ(CAMERA_LENS_FOCUS_DISTANCE, entry.tag);
    EXPECT_EQ(ICAMERA_TYPE_FLOAT, entry.type);
    EXPECT_EQ((size_t)1, entry.count);
    EXPECT_EQ(focus_distance, *entry.data.f);


    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, delete_metadata) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 50;
    const size_t data_capacity = 450;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    size_t num_entries = 5;
    size_t data_per_entry =
            calculate_icamera_metadata_entry_data_size(ICAMERA_TYPE_INT64, 1);
    size_t num_data = num_entries * data_per_entry;

    // Delete an entry with data

    add_test_metadata(m, num_entries);
    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    result = delete_icamera_metadata_entry(m, 1);
    EXPECT_EQ(OK, result);
    num_entries--;
    num_data -= data_per_entry;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    result = delete_icamera_metadata_entry(m, 4);
    EXPECT_EQ(ERROR, result);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    for (size_t i = 0; i < num_entries; i++) {
        icamera_metadata_entry e;
        result = get_icamera_metadata_entry(m, i, &e);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
        int64_t exposureTime = i < 1 ? 100 : 200 + 100 * i;
        EXPECT_EQ(exposureTime, *e.data.i64);
    }

    // Delete an entry with no data, at end of array

    int32_t requestId = 12;
    result = add_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID,
            &requestId, 1);
    EXPECT_EQ(OK, result);
    num_entries++;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    icamera_metadata_entry e;
    result = get_icamera_metadata_entry(m, 4, &e);
    EXPECT_EQ(OK, result);

    EXPECT_EQ((size_t)4, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(requestId, *e.data.i32);

    result = delete_icamera_metadata_entry(m, 4);
    EXPECT_EQ(OK, result);

    num_entries--;
    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    result = delete_icamera_metadata_entry(m, 4);
    EXPECT_EQ(ERROR, result);

    result = get_icamera_metadata_entry(m, 4, &e);
    EXPECT_EQ(ERROR, result);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    // Delete with extra data on end of array
    result = delete_icamera_metadata_entry(m, 3);
    EXPECT_EQ(OK, result);
    num_entries--;
    num_data -= data_per_entry;

    for (size_t i = 0; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = i < 1 ? 100 : 200 + 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Delete without extra data in front of array

    requestId = 1001;
    result = add_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID,
            &requestId, 1);
    EXPECT_EQ(OK, result);
    num_entries++;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    result = sort_icamera_metadata(m);
    EXPECT_EQ(OK, result);

    result = find_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID, &e);
    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(requestId, *e.data.i32);

    result = delete_icamera_metadata_entry(m, e.index);
    EXPECT_EQ(OK, result);
    num_entries--;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    for (size_t i = 0; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = i < 1 ? 100 : 200 + 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }
}

TEST(icamera_metadata, update_metadata) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 50;
    const size_t data_capacity = 450;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    size_t num_entries = 5;
    size_t data_per_entry =
            calculate_icamera_metadata_entry_data_size(ICAMERA_TYPE_INT64, 1);
    size_t num_data = num_entries * data_per_entry;

    add_test_metadata(m, num_entries);
    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    // Update with same-size data, doesn't fit in entry

    int64_t newExposureTime = 1000;
    icamera_metadata_entry_t e;
    result = update_icamera_metadata_entry(m,
            0, &newExposureTime, 1, &e);
    EXPECT_EQ(OK, result);

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newExposureTime, *e.data.i64);

    e.count = 0;
    result = get_icamera_metadata_entry(m,
            0, &e);

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newExposureTime, *e.data.i64);

    for (size_t i = 1; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 + 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update with larger data
    int64_t newExposures[2] = { 5000, 6000 };
    result = update_icamera_metadata_entry(m,
            0, newExposures, 2, &e);
    EXPECT_EQ(OK, result);
    num_data += data_per_entry;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)2, e.count);
    EXPECT_EQ(newExposures[0], e.data.i64[0]);
    EXPECT_EQ(newExposures[1], e.data.i64[1]);

    e.count = 0;
    result = get_icamera_metadata_entry(m,
            0, &e);

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)2, e.count);
    EXPECT_EQ(newExposures[0], e.data.i64[0]);
    EXPECT_EQ(newExposures[1], e.data.i64[1]);

    for (size_t i = 1; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 + 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update with smaller data
    newExposureTime = 100;
    result = update_icamera_metadata_entry(m,
            0, &newExposureTime, 1, &e);
    EXPECT_EQ(OK, result);

    num_data -= data_per_entry;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newExposureTime, *e.data.i64);

    e.count = 0;
    result = get_icamera_metadata_entry(m,
            0, &e);

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newExposureTime, *e.data.i64);

    for (size_t i = 1; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 + 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update with size fitting in entry

    int32_t requestId = 1001;
    result = add_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID,
            &requestId, 1);
    EXPECT_EQ(OK, result);
    num_entries++;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(entry_capacity, get_icamera_metadata_entry_capacity(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));
    EXPECT_EQ(data_capacity, get_icamera_metadata_data_capacity(m));

    result = sort_icamera_metadata(m);
    EXPECT_EQ(OK, result);

    result = find_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID, &e);
    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(requestId, *e.data.i32);

    int32_t newRequestId = 0x12349876;
    result = update_icamera_metadata_entry(m,
            0, &newRequestId, 1, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    result = find_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    for (size_t i = 1; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update to bigger than entry

    int32_t newFrameCounts[4] = { 0x0, 0x1, 0x10, 0x100 };

    result = update_icamera_metadata_entry(m,
            0, &newFrameCounts, 4, &e);

    EXPECT_EQ(OK, result);

    num_data += calculate_icamera_metadata_entry_data_size(ICAMERA_TYPE_INT32,
            4);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)4, e.count);
    EXPECT_EQ(newFrameCounts[0], e.data.i32[0]);
    EXPECT_EQ(newFrameCounts[1], e.data.i32[1]);
    EXPECT_EQ(newFrameCounts[2], e.data.i32[2]);
    EXPECT_EQ(newFrameCounts[3], e.data.i32[3]);

    e.count = 0;

    result = find_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)4, e.count);
    EXPECT_EQ(newFrameCounts[0], e.data.i32[0]);
    EXPECT_EQ(newFrameCounts[1], e.data.i32[1]);
    EXPECT_EQ(newFrameCounts[2], e.data.i32[2]);
    EXPECT_EQ(newFrameCounts[3], e.data.i32[3]);

    for (size_t i = 1; i < num_entries; i++) {
        icamera_metadata_entry e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update to smaller than entry
    result = update_icamera_metadata_entry(m,
            0, &newRequestId, 1, &e);

    EXPECT_EQ(OK, result);

    num_data -= icamera_metadata_type_size[ICAMERA_TYPE_INT32] * 4;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    result = find_icamera_metadata_entry(m,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    for (size_t i = 1; i < num_entries; i++) {
        icamera_metadata_entry_t e2;
        result = get_icamera_metadata_entry(m, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Setup new buffer with no spare data space

    result = update_icamera_metadata_entry(m,
            1, newExposures, 2, &e);
    EXPECT_EQ(OK, result);

    num_data += data_per_entry;

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m));

    EXPECT_EQ((size_t)1, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)2, e.count);
    EXPECT_EQ(newExposures[0], e.data.i64[0]);
    EXPECT_EQ(newExposures[1], e.data.i64[1]);

    icamera_metadata_t *m2;
    m2 = allocate_icamera_metadata(get_icamera_metadata_entry_count(m),
            get_icamera_metadata_data_count(m));
    EXPECT_NOT_NULL(m2);

    result = append_icamera_metadata(m2, m);
    EXPECT_EQ(OK, result);

    result = find_icamera_metadata_entry(m2,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    // Update when there's no more room

    result = update_icamera_metadata_entry(m2,
            0, &newFrameCounts, 4, &e);
    EXPECT_EQ(ERROR, result);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m2));

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    // Update when there's no data room, but change fits into entry

    newRequestId = 5;
    result = update_icamera_metadata_entry(m2,
            0, &newRequestId, 1, &e);
    EXPECT_EQ(OK, result);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m2));

    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    result = find_icamera_metadata_entry(m2,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    result = get_icamera_metadata_entry(m2, 1, &e);
    EXPECT_EQ((size_t)1, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)2, e.count);
    EXPECT_EQ(newExposures[0], e.data.i64[0]);
    EXPECT_EQ(newExposures[1], e.data.i64[1]);

    for (size_t i = 2; i < num_entries; i++) {
        icamera_metadata_entry_t e2;
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update when there's no data room, but data size doesn't change

    newExposures[0] = 1000;

    result = update_icamera_metadata_entry(m2,
            1, newExposures, 2, &e);
    EXPECT_EQ(OK, result);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m2));

    EXPECT_EQ((size_t)1, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)2, e.count);
    EXPECT_EQ(newExposures[0], e.data.i64[0]);
    EXPECT_EQ(newExposures[1], e.data.i64[1]);

    result = find_icamera_metadata_entry(m2,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    for (size_t i = 2; i < num_entries; i++) {
        icamera_metadata_entry_t e2;
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

    // Update when there's no data room, but data size shrinks

    result = update_icamera_metadata_entry(m2,
            1, &newExposureTime, 1, &e);
    EXPECT_EQ(OK, result);

    num_data -= calculate_icamera_metadata_entry_data_size(ICAMERA_TYPE_INT64, 2);
    num_data += calculate_icamera_metadata_entry_data_size(ICAMERA_TYPE_INT64, 1);

    EXPECT_EQ(num_entries, get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(num_data, get_icamera_metadata_data_count(m2));

    EXPECT_EQ((size_t)1, e.index);
    EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT64, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newExposureTime, e.data.i64[0]);

    result = find_icamera_metadata_entry(m2,
            CAMERA_REQUEST_ID, &e);

    EXPECT_EQ(OK, result);
    EXPECT_EQ((size_t)0, e.index);
    EXPECT_EQ(CAMERA_REQUEST_ID, e.tag);
    EXPECT_EQ(ICAMERA_TYPE_INT32, e.type);
    EXPECT_EQ((size_t)1, e.count);
    EXPECT_EQ(newRequestId, *e.data.i32);

    for (size_t i = 2; i < num_entries; i++) {
        icamera_metadata_entry_t e2;
        result = get_icamera_metadata_entry(m2, i, &e2);
        EXPECT_EQ(OK, result);
        EXPECT_EQ(i, e2.index);
        EXPECT_EQ(CAMERA_SENSOR_EXPOSURE_TIME, e2.tag);
        EXPECT_EQ(ICAMERA_TYPE_INT64, e2.type);
        int64_t exposureTime = 100 * i;
        EXPECT_EQ(exposureTime, *e2.data.i64);
    }

}

TEST(icamera_metadata, memcpy) {
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 50;
    const size_t data_capacity = 450;

    int result;

    m = allocate_icamera_metadata(entry_capacity, data_capacity);

    add_test_metadata(m, 5);

    size_t m_size = get_icamera_metadata_size(m);
    uint8_t *dst = new uint8_t[m_size];

    memcpy(dst, m, m_size);

    icamera_metadata_t *m2 = reinterpret_cast<icamera_metadata_t*>(dst);

    ASSERT_EQ(get_icamera_metadata_size(m),
            get_icamera_metadata_size(m2));
    EXPECT_EQ(get_icamera_metadata_compact_size(m),
            get_icamera_metadata_compact_size(m2));
    ASSERT_EQ(get_icamera_metadata_entry_count(m),
            get_icamera_metadata_entry_count(m2));
    EXPECT_EQ(get_icamera_metadata_entry_capacity(m),
            get_icamera_metadata_entry_capacity(m2));
    EXPECT_EQ(get_icamera_metadata_data_count(m),
            get_icamera_metadata_data_count(m2));
    EXPECT_EQ(get_icamera_metadata_data_capacity(m),
            get_icamera_metadata_data_capacity(m2));

    icamera_metadata_entry_t e1, e2;
    for (size_t i = 0; i < get_icamera_metadata_entry_count(m); i++) {
        result = get_icamera_metadata_entry(m, i, &e1);
        ASSERT_EQ(OK, result);
        result = get_icamera_metadata_entry(m2, i, &e2);
        ASSERT_EQ(OK, result);

        EXPECT_EQ(e1.index, e2.index);
        EXPECT_EQ(e1.tag, e2.tag);
        ASSERT_EQ(e1.type, e2.type);
        ASSERT_EQ(e1.count, e2.count);

        ASSERT_TRUE(!memcmp(e1.data.u8, e2.data.u8,
                        icamera_metadata_type_size[e1.type] * e1.count));
    }

    // Make sure updating one metadata buffer doesn't change the other

    int64_t double_exposure_time[] = { 100, 200 };

    result = update_icamera_metadata_entry(m, 0,
            double_exposure_time,
            sizeof(double_exposure_time)/sizeof(int64_t), NULL);
    EXPECT_EQ(OK, result);

    result = get_icamera_metadata_entry(m, 0, &e1);
    ASSERT_EQ(OK, result);
    result = get_icamera_metadata_entry(m2, 0, &e2);
    ASSERT_EQ(OK, result);

    EXPECT_EQ(e1.index, e2.index);
    EXPECT_EQ(e1.tag, e2.tag);
    ASSERT_EQ(e1.type, e2.type);
    ASSERT_EQ((size_t)2, e1.count);
    ASSERT_EQ((size_t)1, e2.count);
    EXPECT_EQ(100, e1.data.i64[0]);
    EXPECT_EQ(200, e1.data.i64[1]);
    EXPECT_EQ(100, e2.data.i64[0]);

    // And in the reverse direction as well

    double_exposure_time[0] = 300;
    result = update_icamera_metadata_entry(m2, 0,
            double_exposure_time,
            sizeof(double_exposure_time)/sizeof(int64_t), NULL);
    EXPECT_EQ(OK, result);

    result = get_icamera_metadata_entry(m, 0, &e1);
    ASSERT_EQ(OK, result);
    result = get_icamera_metadata_entry(m2, 0, &e2);
    ASSERT_EQ(OK, result);

    EXPECT_EQ(e1.index, e2.index);
    EXPECT_EQ(e1.tag, e2.tag);
    ASSERT_EQ(e1.type, e2.type);
    ASSERT_EQ((size_t)2, e1.count);
    ASSERT_EQ((size_t)2, e2.count);
    EXPECT_EQ(100, e1.data.i64[0]);
    EXPECT_EQ(200, e1.data.i64[1]);
    EXPECT_EQ(300, e2.data.i64[0]);
    EXPECT_EQ(200, e2.data.i64[1]);

    EXPECT_EQ(OK, validate_icamera_metadata_structure(m2, &m_size));

    delete dst;
    FINISH_USING_CAMERA_METADATA(m);
}

TEST(icamera_metadata, data_alignment) {
    // Verify that when we store the data, the data aligned as we expect
    icamera_metadata_t *m = NULL;
    const size_t entry_capacity = 50;
    const size_t data_capacity = 450;
    char dummy_data[data_capacity] = {0,};

    int m_types[] = {
        ICAMERA_TYPE_BYTE,
        ICAMERA_TYPE_INT32,
        ICAMERA_TYPE_FLOAT,
        ICAMERA_TYPE_INT64,
        ICAMERA_TYPE_DOUBLE,
        ICAMERA_TYPE_RATIONAL
    };
    const size_t (&m_type_sizes)[ICAMERA_NUM_TYPES] = icamera_metadata_type_size;
    size_t m_type_align[] = {
        _Alignas(uint8_t),                    // BYTE
        _Alignas(int32_t),                    // INT32
        _Alignas(float),                      // FLOAT
        _Alignas(int64_t),                    // INT64
        _Alignas(double),                     // DOUBLE
        _Alignas(icamera_metadata_rational_t), // RATIONAL
    };
    /* arbitrary tags. the important thing is that their type
       corresponds to m_type_sizes[i]
       */
    int m_type_tags[] = {
        CAMERA_REQUEST_METADATA_MODE,
        CAMERA_REQUEST_ID,
        CAMERA_LENS_FOCUS_DISTANCE,
        CAMERA_SENSOR_EXPOSURE_TIME,
        CAMERA_JPEG_GPS_COORDINATES,
        CAMERA_AE_COMPENSATION_STEP
    };

    /*
    if the asserts fail, its because we added more types.
        this means the test should be updated to include more types.
    */
    ASSERT_EQ((size_t)ICAMERA_NUM_TYPES, sizeof(m_types)/sizeof(m_types[0]));
    ASSERT_EQ((size_t)ICAMERA_NUM_TYPES, sizeof(m_type_align)/sizeof(m_type_align[0]));
    ASSERT_EQ((size_t)ICAMERA_NUM_TYPES, sizeof(m_type_tags)/sizeof(m_type_tags[0]));

    for (int m_type = 0; m_type < (int)ICAMERA_NUM_TYPES; ++m_type) {

        ASSERT_EQ(m_types[m_type],
            get_icamera_metadata_tag_type(m_type_tags[m_type]));

        // misalignment possibilities are [0,type_size) for any type pointer
        for (size_t i = 0; i < m_type_sizes[m_type]; ++i) {

            /* data_count = 1, we may store data in the index.
               data_count = 10, we will store data separately
             */
            for (int data_count = 1; data_count <= 10; data_count += 9) {

                m = allocate_icamera_metadata(entry_capacity, data_capacity);

                // add dummy data to test various different padding requirements
                ASSERT_EQ(OK,
                    add_icamera_metadata_entry(m,
                                              m_type_tags[ICAMERA_TYPE_BYTE],
                                              &dummy_data[0],
                                              data_count + i));
                // insert the type we care to test
                ASSERT_EQ(OK,
                    add_icamera_metadata_entry(m, m_type_tags[m_type],
                                             &dummy_data[0], data_count));

                // now check the alignment for our desired type. it should be ok
                icamera_metadata_ro_entry_t entry = icamera_metadata_ro_entry_t();
                ASSERT_EQ(OK,
                    find_icamera_metadata_ro_entry(m, m_type_tags[m_type],
                                                 &entry));

                void* data_ptr = (void*)entry.data.u8;
                void* aligned_ptr = (void*)((uintptr_t)data_ptr & ~(m_type_align[m_type] - 1));
                EXPECT_EQ(aligned_ptr, data_ptr) <<
                    "Wrong alignment for type " <<
                    icamera_metadata_type_names[m_type] <<
                    " with " << (data_count + i) << " dummy bytes and " <<
                    " data_count " << data_count <<
                    " expected alignment was: " << m_type_align[m_type];

                EXPECT_EQ(8, get_icamera_metadata_alignment());

                FINISH_USING_CAMERA_METADATA(m);
            }
        }
    }
}


TEST(icamera_metadata, error_branch) {
    const size_t expected_size = 8;
    EXPECT_EQ(ERROR, validate_icamera_metadata_structure(nullptr, &expected_size));

    const size_t entry_capacity = 5;
    const size_t data_capacity = 32;
    icamera_metadata_t *m = allocate_icamera_metadata(entry_capacity, data_capacity);
    size_t memory_needed = calculate_icamera_metadata_size(entry_capacity - 1,
                                                              data_capacity);
    // test a case that metadata size > expected size.
    EXPECT_EQ(ERROR, validate_icamera_metadata_structure(m, &memory_needed));
}
