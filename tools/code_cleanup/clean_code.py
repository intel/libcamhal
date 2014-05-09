#!/usr/bin/python3

"""
This script delete all the code that are not used in Chrome.

Usage: ./script_name excluded_features_config_filename source_code_directory
"""

import sys
import os
import shutil
import re
import yaml
import glob

def flattern(A):
    rt = []
    for i in A:
        if isinstance(i,list):
            rt.extend(flattern(i))
        else:
            rt.append(i)
    return rt

def get_exclude_dict(conf_file):
    exclude_dict = []
    with open(conf_file) as fd:
            exclude_dict = yaml.safe_load(fd)
    return exclude_dict


def getSourceFiles(directory, fileappendix):
    for parent, dirnames, filenames in os.walk(directory):
        for filename in filenames:
            if filename.endswith(fileappendix):
                yield os.path.join(parent, filename)


def analyzeLine(start, end, line):
    start_flag, end_flag = False, False

    if line.find(start) >= 0:
        start_flag = True
    elif line.find(end) >= 0:
        end_flag = True

    return start_flag, end_flag


# Remove the feature in the lines buffer and return the result
def remove_feature(start, end, lines):
    newlines = []
    skip = False
    insideFeature = False
    for line in lines:
        xline = line.strip()
        startTag, endTag = analyzeLine(start, end, xline)
        if startTag:
            if insideFeature:
                print("Warning: Duplicated Start Tag")
            skip = True
            insideFeature = True
            continue
        if endTag:
            if not insideFeature:
                newlines.append(line)
                continue

            skip = False
            insideFeature = False
            continue
        # Replace #else with #ifndef to process later
        if xline.startswith("#else") and start.startswith("#ifdef") and insideFeature:
            newlines.append(start.replace("#ifdef", "#ifndef"))
            insideFeature = False
            skip = False
            continue

        # For Makefile.am, replace "else" with "if NOT" to process later
        if xline.startswith("else") and start.startswith("if ") and insideFeature:
            newlines.append(start.replace("if", "if NOT"))
            insideFeature = False
            skip = False
            continue

        if not skip:
            newlines.append(line)

    return newlines


# Remove the #ifndef and #endif keyword in the lines buffer and return the result
def remove_ifndef(start, end, lines):
    if start.startswith("#ifdef"):
        new_start = start.replace("#ifdef", "#ifndef")
    if start.startswith("if "):
        new_start = start.replace("if ", "if NOT ")
    if start.startswith("if ("):
        new_start = start.replace("if (", "if (NOT ")

    newlines = []
    insideifndef = False
    for line in lines:
        xline = line.strip()
        startTag, endTag = analyzeLine(new_start, end, xline)
        if startTag:
            if insideifndef:
                print("Warning: Duplicated Start Tag")
            insideifndef = True
            continue
        if endTag:
            if not insideifndef:
                newlines.append(line)
                continue

            insideifndef = False
            continue

        newlines.append(line)

    return newlines


# Iterate all the source file to remove the features between start and end
def clean_feature(start, end, root_dir, fileappendix):
    sourceFiles = getSourceFiles(root_dir, fileappendix)
    for src in sourceFiles:
        if not os.path.isfile(src):
            continue
        with open(src) as f:
            lines = f.readlines()

        new_lines = remove_feature(start, end, lines)

        if start.startswith("#ifdef") or start.startswith("if ") or start.startswith("if ("):
            new_lines = remove_ifndef(start, end, new_lines)

        with open(src, 'w') as f:
            for l in new_lines:
                f.write(l)


def clean_all_features(f_list, root_dir, fileappendix):
    for feature in f_list:
        print("start processing {}".format(feature))
        f_start = feature.strip() + "_S"
        f_end = feature.strip() + "_E"
        clean_feature(f_start, f_end, root_dir, fileappendix)


# Delete all the code between "#ifdef xxx; #endif"
def clean_all_ifdefs(ifdefs_list, root_dir, fileappendix):
    print("start processing {}".format(ifdefs_list))
    for ifdef in ifdefs_list:
        # clenaup source code
        f_start = "#ifdef " + ifdef
        f_end = "#endif"
        clean_feature(f_start, f_end, root_dir, fileappendix)

        # clenaup Makefile.am
        f_start = "if " + ifdef
        f_end = "endif #" + ifdef
        clean_feature(f_start, f_end, root_dir, (".am"))

        # cleanup CMakeFile.txt
        f_start = "if (" + ifdef + ")"
        f_end = "endif() #" + ifdef
        clean_feature(f_start, f_end, root_dir, (".txt", "*.cmake"))


def delete_all_folders(f_list, root_dir):
    for folder in f_list:
        real_folder = os.path.join(root_dir, folder)
        print("Start delete {}".format(real_folder))
        try:
            shutil.rmtree(real_folder, ignore_errors=True)
        except OSError:
            print("cannot open", real_folder)

def delete_all_files(f_list, root_dir):
    new_list = []
    for fi in f_list:
        full_path = os.path.join(root_dir, fi)
        new_list.append(glob.glob(full_path, recursive=True))

    full_list = flattern(new_list)
    for real_file in full_list:
        print("Start delete {}".format(real_file))
        try:
            os.remove(real_file)
        except OSError:
            print("cannot open", real_file)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage:{} excluded_features_config_filename".format(sys.argv[0]))
        sys.exit(-1)

    source_code_dir = ["../../"]

    if len(sys.argv) >= 3:
        source_code_dir = sys.argv[2:]

    exclude_dict = get_exclude_dict(sys.argv[1])

    root_dir = source_code_dir[0].strip()

    # Prevent to remove the root directory
    if re.match(r"/*$", root_dir):
        print("Root directory should not be /")
        sys.exit()

    fileAppendix = tuple(exclude_dict['FileAppendix'])

    # Delelte all unix style pathname files
    if "Files" in exclude_dict:
        delete_all_files(exclude_dict['Files'], root_dir)

    # Delelte all Folders files
    if "Folders" in exclude_dict:
        delete_all_folders(exclude_dict['Folders'], root_dir)

    # Delete all the Features
    if "Features" in exclude_dict:
        clean_all_features(exclude_dict['Features'], root_dir, fileAppendix)

    # Delete all the ifdefs
    if "Ifdefs" in exclude_dict:
        clean_all_ifdefs(exclude_dict['Ifdefs'], root_dir, fileAppendix)
