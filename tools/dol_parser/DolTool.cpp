/*
 * Copyright (C) 2017 Intel Corporation.
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

#include <iostream>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <errno.h>

#define ALIGN(val, alignment) (((val)+(alignment)-1) & ~((alignment)-1))
#define ALIGN_64(val) ALIGN(val, 64)

#define OUTPUT_LONG_EXPOSURE_FILE "output_long.raw"
#define OUTPUT_SHORT_EXPOSURE_FILE "output_short.raw"
#define PARSE_FILE_NUM 2

using namespace std;

static const struct option opts[] = {
    {"file-name", 1, NULL, 'f'},
    {"format", 1, NULL, 'F'},
    {"width", 1, NULL, 'W'},
    {"height", 1, NULL, 'H'},
    {"offset", 1, NULL, 'O'},
    {"help", 1, NULL, 'h'},
    {0, 0, 0, 0}
};

static void usage(const char *argv0)
{
    printf("Supported options:\n");
    printf("-f, --file-name         The raw data file which need to be parsed\n");
    printf("-F, --format            Raw data format\n");
    printf("-W, --width             Raw data width\n");
    printf("-H, --height            Raw data height\n");
    printf("-O, --offset            Raw data offset\n");
    printf("-h, --help              print help\n");
    printf("Usage: %s -f filename -F format -W width -H height -O offset\n", argv0);
}

int getBppByFormatString(const char *format)
{
    if (format == NULL) {
        printf("use default format: raw12, bpp: 16\n");
        return 16;
    }
    if (strcmp(format, "raw12") == 0 ||
            strcmp(format, "raw10") == 0) {
        return 16;
    } else if (strcmp(format, "raw8") == 0) {
        return 8;
    } else if (strcmp(format, "raw10p") == 0) {
        return 10;
    } else {
        printf("warning: don't support format: %s, use default bpp\n", format);
    }

    return 16;
}

int parseDolRawData(const char *filename, int width, int height, int offset, int bpp)
{
    int ret = 0;
    char *pBuf = NULL;
    int writeSLine = 0;
    int bufferLen = ALIGN_64(width * bpp / 8);
    size_t readLen = 0, writeLLen = 0, writeSLen = 0;
    int lineIndex = -1, writeLIndex = 0, writeSIndex = 0;
    long int writeLSize = 0, writeSSize = 0;
    FILE *inputFp = NULL, *outputLongFp = NULL, *outputShortFp = NULL;

    inputFp = fopen(filename, "r");
    if (inputFp == NULL) {
        printf("Failed to open the input file: %s  %s\n", filename, strerror(errno));
        ret = -1;
        goto exit;
    }
    outputLongFp = fopen(OUTPUT_LONG_EXPOSURE_FILE, "w+");
    if (outputLongFp == NULL) {
        printf("Failed to open the output long file: %s\n", strerror(errno));
        ret = -1;
        goto exit;
    }
    outputShortFp = fopen(OUTPUT_SHORT_EXPOSURE_FILE, "w+");
    if (outputShortFp == NULL) {
        printf("Failed to open the output short file: %s\n", strerror(errno));
        ret = -1;
        goto exit;
    }

    pBuf = new char[bufferLen];
    do {
        readLen = fread(pBuf, 1, bufferLen, inputFp);
        if (readLen < 0) {
            printf("Read data error. filename: %s, %s", filename, strerror(errno));
            ret = -1;
            goto exit;
        }
        lineIndex++;
        if (readLen != bufferLen) {
            printf("the bufferLen: %d, readLen: %lu, at times: %d\n", bufferLen, readLen, lineIndex);
            break;
        }
        if (lineIndex % PARSE_FILE_NUM == 0 && writeLIndex < height) {
            writeLLen = fwrite(pBuf, 1, bufferLen, outputLongFp);
            if (writeLLen != bufferLen) {
                printf("%zu, error to write the output long file\n", writeLLen);
                ret = -1;
                goto exit;
            }
            writeLSize += writeLLen;

            writeLIndex++;
        } else {
            writeSIndex++;
            //skip the offset line and only write height times
            if ((writeSIndex <= offset) || writeSLine >= height)
                continue;

            //skip the empty lines when write long exposure file end.
            if ((writeLIndex >= height) && lineIndex % PARSE_FILE_NUM == 0)
                continue;

            writeSLen = fwrite(pBuf, 1, bufferLen, outputShortFp);
            if (writeSLen != bufferLen) {
                printf("%zu, error to write the output short file\n", writeSLen);
                ret = -1;
                goto exit;
            }
            writeSLine += 1;
            writeSSize += writeSLen;
        }
    } while(!(readLen < bufferLen));

    printf("long exposure size: %ld, lines: %d, short exposure size: %ld, lines: %d\n", writeLSize, writeLIndex, writeSSize, writeSLine);

exit:
    if (inputFp)
        fclose(inputFp);
    if (outputLongFp)
        fclose(outputLongFp);
    if (outputShortFp)
        fclose(outputShortFp);
    if (pBuf) {
        delete [] pBuf;
        pBuf = NULL;
    }

    return ret;
}

int main(int argc, char *argv[])
{
    int ret = 0;
    int c = 0, bpp = 0;
    char *inputFile = NULL, *format = NULL;
    int width = 0, height = 0, offset = 0;

    //Need to input all necessary parameters which use to parse the dol data
    if (argc < 5) {
        printf("%d, Please input the necessary parameters\n", __LINE__);
        usage(argv[0]);
        return -1;
    }

    while ((c = getopt_long(argc, argv, "f:F:W:H:O:h", opts, NULL)) != -1) {
        switch (c) {
        case 'f':
            if (optarg)
                inputFile = optarg;
            break;
        case 'F':
            if (optarg)
                format = optarg;
            break;
        case 'W':
            if (optarg)
                width = atoi(optarg);
            break;
        case 'H':
            if (optarg)
                height = atoi(optarg);
            break;
        case 'O':
            if (optarg)
                offset = atoi(optarg);
            break;
        case 'h':
            usage(argv[0]);
            break;
        default:
            printf("Invalid option -%c\n", c);
            printf("Run %s -h for help.\n", argv[0]);
            return -1;
        }
    }

    if (inputFile == NULL || width == 0 || height == 0 || offset == 0) {
        printf("%d, Please input the necessary parameters\n", __LINE__);
        usage(argv[0]);
        return -1;
    }
    bpp = getBppByFormatString(format);
    printf("filename: %s, width:%d, height: %d, bpp: %d, offset:%d\n",
                                        inputFile, width, height, bpp, offset);

    ret = parseDolRawData(inputFile, width, height, offset, bpp);
    if (ret != 0) {
        printf("Error to decompose the dol raw data\n");
        return -1;
    }

    return 0;
}
