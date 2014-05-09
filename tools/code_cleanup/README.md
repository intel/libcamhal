# Purpose of this Folder
This folder provides scripts to release code for different projects by
1. Cleaning features inside the file
2. Delete some files that is not used in this project
3. Delete some folders that is not used in this project
4. Delete features start with "#ifdef xxx" and end with "#endif". "#ifdef-#else-
   #endif" is supported. But NOTE that "#ifndef-#else-#endif" is NOT supported.

Please update the "exclude_conf.json" for your project to use this script.

# How to add TAGs to code
Code should have following specific tags as start and end.
 * LITE_PROCESSING (for weaving, scaling, dewarping)
 * LOCAL_TONEMAP  (FOR HDR and ULL)

**More flags could be added in the future.**

By running the script "./remove_features.py exclude_conf.json", all the code
between a feature's start tag(end with "_S") and its end tag(ends with "_E")
will be deleted.
For example, the code between "UNDESIRED_FEATURE_S" and "UNDESIRED_FEATURE_E"
will be deleted.

Sample usage for remove the code related to feature called "UNDESIRED_FEATURE":
// UNDESIRED_FEATURE_S
int getIsysConfig(int width, int height)
{
    return 0;
}
// UNDESIRED_FEATURE_E

# Dependencies
Some features have dependency to other features, once the depended one is excluded,
the depending one should also be excluded.
 * LITE_PROCESSING depends on INTEL_DVS
 * CUSTOMIZED_3A   depends on LOCAL_TONEMAP
