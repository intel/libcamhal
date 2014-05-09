/*
 * Copyright (C) 2018 Intel Corporation.
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

/**
 *\File PolicyProfiles.h
 *
 * parser for the policy xml configuration file
 *
 * This file calls the libexpat ditectly. The libexpat is one xml parser.
 * It will parse the camera configuration out firstly.
 * Then other module can call the methods of it to get the real configuration.
 */

#pragma once

#include "iutils/Utils.h"

#include "CameraTypes.h"
#include "PlatformData.h"

namespace icamera {

/**
 * \class PolicyProfiles
 *
 * This class is used to parse the policy configuration file.
 * The configuration file is xml format.
 * This class will use the expat lib to do the xml parser.
 */
class PolicyProfiles {
public:
    PolicyProfiles(PlatformData::StaticCfg *cfg);
    ~PolicyProfiles(){};

private:
    // prevent copy constructor and assignment operator
    DISALLOW_COPY_AND_ASSIGN(PolicyProfiles);

private:
    static void startElement(void *userData, const char *name, const char **atts);
    static void endElement(void *userData, const char *name);
    void getAvaliableXmlFile(const vector<const char *>& avaliableXmlFiles, string &xmlFile);
    void parseXmlFile(const string &xmlFile);
    void getPolicyConfigFromXmlFile(void);
    void checkField(PolicyProfiles *profiles, const char *name, const char **atts);

    void handlePolicyConfig(PolicyProfiles *profiles, const char *name, const char **atts);

    int parsePgList(const char *str, vector<string> &pgList);
    int parseOpModeList(const char *str, vector<int> &opModeList);
    int parseCyclicFeedbackRoutineList(const char *str, vector<int> &cyclicFeedbackRoutineList);
    int parseCyclicFeedbackDelayList(const char *str, vector<int> &cyclicFeedbackDelayList);
    void handlePipeExecutor(PolicyProfiles *profiles, const char *name, const char **atts);
    void handleExclusivePGs(PolicyProfiles *profiles, const char *name, const char **atts);
    void handleBundles(PolicyProfiles *profiles, const char *name, const char **atts);

    //Skip whitespace. (space, tab, newline, vertical tab, feed, carriage return)
    const char* skipWhiteSpace(const char *src);

private:
    enum DataField {
        FIELD_INVALID = 0,
        FIELD_GRAPH,
    } mCurrentDataField;
    static const int mBufSize = 4*1024;
    PlatformData::StaticCfg* mStaticCfg;
    PolicyConfig *pCurrentConf;
};

} // namespace icamera
