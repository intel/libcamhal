#!/usr/bin/python
#
#  Copyright (C) 2016 Intel Corporation
#
#  Licensed under the Apache License, Version 2.0 (the "License");
#  you may not use this file except in compliance with the License.
#  You may obtain a copy of the License at
#
#       http://www.apache.org/licenses/LICENSE-2.0
#
#  Unless required by applicable law or agreed to in writing, software
#  distributed under the License is distributed on an "AS IS" BASIS,
#  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
#  See the License for the specific language governing permissions and
#  limitations under the License.
#
import os
import re
import sys
import json
import argparse

class NoFileError(Exception):
    pass

class Color_Print(object):

    @staticmethod
    def red(cls):
        print "\033[1;31;40m%s \033[0m" % cls

    @staticmethod
    def green(cls):
        print "\033[1;32;40m%s \033[0m" % cls

    @staticmethod
    def yellow(cls):
        print "\033[1;33;40m%s \033[0m" % cls

def summary(filename, casename_set):
    '''Summarize the testing results in brief'''
    fail_list=[]
    result_list=[]
    fail_result=re.compile("^\[\s\sFAILED\s\s\]\s[0-9].*")
    for elem in casename_set:
        num = get_linenum(elem, filename)
        if len(num)<2:
            fail_list.append(elem)

    try:
        with open(filename, 'r') as f:
            for line in f.readlines():
                if line.find("FAILED") != -1:
                    res=fail_result.match(line)
                    if not res:
                        fail_list.append(line)
                    else:
                        result_list.append(line)
                elif line.find("PASSED") != -1:
                    result_list.append(line)
    except IOError as err:
        raise NoFileError("Unable to read file: %s" % filename)

    s="%s %s testing result:" % (filename.split('_')[0], filename.split('_')[1].split('.')[0])
    Color_Print.green(s)

    if not result_list:
        if fail_list:
            s="%s %s testing terminate abnormal, stop at:" % (filename.split('_')[0], filename.split('_')[1].split('.')[0])
            Color_Print.red(s)
            for m in fail_list:
                print "   %s" % m.strip(' \n').split(' ')[-1]
    else:
        for i in result_list:
            if i.find("FAILED") != -1:
                Color_Print.red(i.strip(' \n'))
            else:
                print i.strip(' \n')
        for m in fail_list:
            print "   %s" % m.strip(' \n').split(' ')[-1]

def get_casename(filename):
    '''Get case name from log'''
    casename_set = set()
    try:
        with open(filename, 'r') as f:
            for line in f.readlines():
                if re.search(r'cam.*Test\.\w',line) or re.search(r'camera_metadata\.',line):
                    l = line.split(' ')
                    for elem in l:
                        if re.search('\w+\.\w+', elem):
                            casename_set.add(elem.strip('\n'))
    except IOError as err:
        raise NoFileError("Unable to read file: %s" % filename)

    return casename_set

def get_linenum(elem, filename):
    '''Get line num of case in testing result log'''
    num = []
    cmd = "awk '/%s/{print NR}' %s" % (elem, filename)
    output = os.popen(cmd)
    for line in output.readlines():
        num.append(int(line.strip('\n')))
    return num


def dump_json(case_error, filename, jsonfile):
    '''Dump dict to json file'''
    if os.path.exists(os.path.abspath(jsonfile)):
        jsobj = load_json(jsonfile)
        for key, value in case_error.items():
            if jsobj.has_key(key):
                jsobj[key].extend(value)
                jsobj[key] = list(set(jsobj[key]))
            else:
                jsobj[key] = value
        json.dump(jsobj, open(jsonfile, 'w'), indent=4)
    else:
        json.dump(case_error, open(jsonfile, 'w'), indent=4)
    s="Cases and corresponding errors already dumped to '%s'." % jsonfile
    Color_Print.green(s)

def load_json(jsonfile):
    '''Load json file'''
    jsobj = json.load(open(jsonfile, 'r'))
    return jsobj

def dispose_log(casename_set, filename, jsonfile=None, f_e=False, f_c=False):
    """
    Dispose log, which including extract errors from testing result log
    to json file for comparing, and compare errors info based on it.

    @param casename_set: set of all case name
    @param filename: testing log
    @param jsonfile: the file dump dict to it
    @param f_e: flag of extract
    @param f_c: flag of compare
    @rtype: two dicts
   """
    def get_errors(filename, num, action, jsobj=None):
        '''Get errors set of each case'''
        errors_set=set()
        try:
            with open(filename, 'r') as f:
                for line in f.readlines()[num[0]:num[1]]:
                    # Donot check line(error) starts with 'CAMHAL_INTERFACE:camera index'
                    if line.find('CAMHAL_INTERFACE:camera index') != -1:
                        continue
                    if line.find('ERROR') != -1:
                        line = line.strip('\n')
                        # Always list 'IAPAL' errors in comparing output result
                        if action == "extract":
                            if line.find('IAPAL') == -1:
                                errors_set.add(' '.join(line.split(' ')[3:]))
                        elif action == "compare":
                            if not jsobj:
                                if line.find('IAPAL') == -1:
                                    errors_set.add(' '.join(line.split(' ')[3:]))
                                else:
                                    errors_set.add(line)
                            else:
                                if line.find('IAPAL') == -1:
                                    line=' '.join(line.split(' ')[3:])
                                    if line not in jsobj[elem]:
                                        errors_set.add(line)
                                else:
                                    errors_set.add(line)
        except IOError as err:
            raise NoFileError("Unable to read file: %s" % filename)

        return errors_set

    case_error = {}
    new_case_error = {}

    if casename_set:
        if f_e:
            #### Extract errors
            for elem in casename_set:
                errors = []
                num = get_linenum(elem, filename)
                if len(num)<2:
                    continue
                errors = get_errors(filename, num, "extract")
                if errors:
                    case_error[elem] = list(errors)
            return case_error
        elif f_c:
            #### Compare errors
            if jsonfile:
                jsobj = load_json(jsonfile)
                for elem in casename_set:
                    num = get_linenum(elem, filename)
                    if len(num)<2:
                        continue
                    if elem not in jsobj.keys():
                        ##### Cases not in js file
                        new_errors = get_errors(filename, num, "compare")
                        if new_errors:
                            new_case_error[elem] = list(new_errors)
                    else:
                        #### New errors of cases in js file
                        new_errors = get_errors(filename, num, "compare", jsobj)
                        if new_errors:
                            case_error[elem] = list(new_errors)

                return case_error, new_case_error
            else:
                s="Error: No extracted json file for comparing."
                Color_Print.red(s)
                exit(1)
        else:
            s="Error: please figure out action as execute/compare."
            Color_Print.red(s)
            exit(1)
    else:
        s="Error: no cases from %s to dispose." % filename
        Color_Print.red(s)
        exit(1)

def print_result(case_error, new_case_error, filename):
    '''Print result of comparing result'''
    if new_case_error:
        p_s = "Cases(errors) are out of json file, if nonfatal, please dump them to json file."
        star = '*' * len(p_s + filename)
        s = "%s\n%s: %s\n%s" % (star, filename.split('.')[0].upper(), p_s,  star)
        Color_Print.green(s)
        for key, value in new_case_error.items():
            Color_Print.yellow("%s:" % key)
            for i in value:
                print i
            print "------------------------------"

    if case_error:
        p_s="Errors are out of json file, if nonfatal, please dump them to json file."
        star = '*' * len(p_s + filename)
        s="%s\n%s: %s\n%s" % (star, filename.split('.')[0].upper(), p_s, star)
        Color_Print.green(s)
        for key, value in case_error.items():
            Color_Print.yellow("%s:" % key)
            for i in value:
                print i
            print "------------------------------"

def result_summary(filename):
    casename_set = get_casename(filename)
    summary(filename, casename_set)

def extract_errors(filename, jsonfile=None):
    casename_set = get_casename(filename)
    case_error = dispose_log(casename_set, filename, f_e=True)
    if not jsonfile:
        jsonfile = "camera_compare.json"
    dump_json(case_error, filename, jsonfile)

def compare_errors(filename, jsonfile):
    casename_set = get_casename(filename)
    case_error, new_case_error = dispose_log(casename_set, filename, jsonfile, f_c=True)
    print_result(case_error, new_case_error, filename)

def parse_args():
    '''Parse subcommand and options'''
    parser = argparse.ArgumentParser(description='Analyze Log')
    subparsers = parser.add_subparsers(help="command")
    # Summary command
    summary_parser = subparsers.add_parser('summary', help='Summarize testing result in brief')
    summary_parser.add_argument('-f','--filename',required=True, help='testing result log')
    # Extract command
    extract_parser = subparsers.add_parser('extract', help='Extract errors from testing log to json file')
    extract_parser.add_argument('-f','--filename',required=True, help='testing result log')
    extract_parser.add_argument('-j','--jsonfile',help='specify json file to extract to')
    # Compare command
    compare_parser = subparsers.add_parser('compare', help='Compare errors for each case from testing log based on'\
                                           ' extracted json file')
    compare_parser.add_argument('-f','--filename',required=True, help='testing result log')
    compare_parser.add_argument('-j','--jsonfile',required=True, help='json_file to compare with')

    return parser.parse_args()

if __name__ == "__main__":
    args = parse_args()
    subcom = sys.argv[1:][0]
    if subcom == "summary":
        if args.filename:
            filename = args.filename
        result_summary(filename)
    elif subcom == "extract":
        if args.filename:
            filename = args.filename
        if args.jsonfile:
            jsonfile = args.jsonfile
            extract_errors(filename, jsonfile)
        else:
            extract_errors(filename)
    elif subcom == "compare":
        if args.filename:
            filename = args.filename
        if args.jsonfile:
            jsonfile = args.jsonfile
        compare_errors(filename, jsonfile)
