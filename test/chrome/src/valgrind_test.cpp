/*
 * Copyright (C) 2016-2017 Intel Corporation
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

/*
 * This file has a valgrind test which runs a few of the tests in valgrind mode,
 * and parses the valgrind output to find memory leaks or errors.
 *
 * In the valgrind mode the timeouts of the tests are increased.
 *
 * If you want to run all of the tests under valgrind, you need to disable
 * running of this test by using --gtest_filter=-*Valgrind*, like this:
 * such a case one would simply issue a command like:
 * "valgrind ipu4_unittests --valgrind --gtest_filter=-*Valgrind* sensorname1 sensorname2"
 *  ^ the above would run all tests under valgrind, except the one in this file
 */

#include "gtest/gtest.h"
#include "test_utils.h"
#include <cstdio>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#define NOT_TEST_MEMORY_LEAKS

extern const char *gTestArgv[];
extern const char *gExecutableName;
extern int gTestArgc;

TEST(Valgrind, MemoryLeaks) {
#ifndef NOT_TEST_MEMORY_LEAKS
    char buffer[1024];
    char command[1024];
    bool definitelyLost = true;
    bool indirectlyLost = true;
    bool possiblyLost   = true;
    bool errors         = true;

    std::string arguments;
    for (int i = 0; i < gTestArgc; i++) {
        arguments.append(gTestArgv[i]);
        if (i != gTestArgc - 1)
            arguments.append(" ");
    }

    snprintf(command, sizeof(command),
             "valgrind --suppressions=/usr/share/%s/unittest.supp %s "
             "%s --valgrind --gtest_filter="
             "*Bracketing*:*TestJpegCapture/0:*TestYuv/0 2>&1",
             gExecutableName,
             gExecutableName,
             arguments.c_str());

    std::shared_ptr<FILE> pipe(popen(command, "r"), pclose);
    ASSERT_TRUE(!pipe) << "popen() failed!";
    while (!feof(pipe.get())) {
        if (fgets(buffer, sizeof(buffer), pipe.get()) != NULL) {
            if (strstr(buffer, "definitely lost: 0 bytes in 0 blocks") != NULL)
                definitelyLost = false;
            if (strstr(buffer, "indirectly lost: 0 bytes in 0 blocks") != NULL)
                indirectlyLost = false;
            if (strstr(buffer, "possibly lost: 0 bytes in 0 blocks") != NULL)
                possiblyLost = false;
            if (strstr(buffer, "ERROR SUMMARY: 0 errors from 0 contexts") != NULL)
                errors = false;

            PRINTF("valgrind run: %s", buffer);
        }
    }

    ASSERT_FALSE(definitelyLost);
    ASSERT_FALSE(indirectlyLost);
    ASSERT_FALSE(possiblyLost);
    ASSERT_FALSE(errors);
#else
    return;
#endif
}
