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
import datetime
from gbp.git.repository import GitRepository

class CheckCopyrightError(Exception):
    pass

def get_info_from_repo(dir_path):
    '''Get info object of HEAD patch of git tree'''
    repo = GitRepository(dir_path)
    info = repo.get_commit_info('HEAD')
    return info

def get_pattern_lists(line_list, time):
    '''Get the lists which coincidence re mode'''
    cr_pattern = re.compile(".*(Copyright).*")
    intel_pattern=re.compile(".*(Intel).*")
    time_intel_pattern = re.compile(r'%s Intel' % time)

    glob_ = lambda pattern: [m.group(0) for l in line_list for m in  \
            [pattern.search(l)] if m]

    copyright_list = glob_(cr_pattern)
    intel_list = glob_(intel_pattern)
    time_intel_list = glob_(time_intel_pattern)

    return [copyright_list, intel_list, time_intel_list]

def get_copyright_template(suffix):
    '''Determine what copyright template to use'''
    star_comments=['h', 'cpp', 'c', 'cc', 'java', 'json', 'txt']
    poundsign_comments=['Makefile', 'py', 'am', 'sh', 'mk', 'README']

    cur_dir = os.path.abspath('.')

    if suffix in star_comments:
        cr_file_path = os.path.join(cur_dir, 'copyright_star_template')
    elif suffix in poundsign_comments:
        cr_file_path = os.path.join(cur_dir, 'copyright_poundsign_template')
    elif suffix == 'xml':
        cr_file_path = os.path.join(cur_dir, 'copyright_xml_template')
    else:
        return ""

    return cr_file_path

def get_update_cmd(f, copyright_list, intel_list, time_intel_list, time):
    suffix = (f.split('/')[-1]).split('.')[-1]
    cmd = ['./update.sh', f]
    cmd += [get_copyright_template(suffix)]

    if copyright_list:
        if not intel_list:
            cmd += [str(time), "true", "false"]
            return cmd
        elif not time_intel_list:
            cmd += [str(time), "false", "true"]
            return cmd
        else:
            pass
    else:
        cmd += [str(time)]
        return cmd

def check_copyright(dir_path, f):
    """
    Check copyright for files according to the infomation from the HEAD
    patch, mainly focus on email and submitted year.

    @param f: file needs to be checked
    @param dir_path: directory path of f
    @return: command(for updating) or pass
    @rtype: str
    """
    info=get_info_from_repo(dir_path)

    email_ = lambda key_w: re.search("\w+\.\w+@intel.*$", info[key_w].email)
    time_ = lambda key_w: datetime.datetime.fromtimestamp(int(info[key_w].date.split()[0]))

    committer_email = email_('committer')
    author_email = email_('author')
    committer_time = time_('committer')
    author_time = time_('author')

    with open(f, 'r') as file_hd:
        linelist = [line.strip() for line in file_hd.readlines()[0:7]]

        if committer_email:
            cr_l, i_l, t_i_l = get_pattern_lists(linelist, committer_time.year)
            return get_update_cmd(f, cr_l, i_l, t_i_l, committer_time.year)
        elif author_email:
            cr_l, i_l, t_i_l = get_pattern_lists(linelist, author_time.year)
            return get_update_cmd(f, cr_l, i_l, t_i_l, author_time.year)
        else:
            print "No intel committer or author from HEAD patch."

def go_through_all_files(dir_path):
    """
    Go throught all files under dir_path, filter file format by suffix.

    @param dir_path: relative or absolute directory path
    @return: all eligible files
    @rtype: list
    """
    def filter_file(f):
        suffix_pattern = re.compile(r'^h$|^c$|^cc$|^cpp$|^java$|^json$|^txt$|^py$|^am$|^sh$|^mk$|^Makefile$|^xml$|^README$')
        r = suffix_pattern.match(f.split('.')[-1])
        if not r:
            return None
        else:
            return r

    files_set = set()
    for root, dirs, files in os.walk(dir_path):
        for filespath in files:
            if filter_file(filespath) and not re.findall(r'include\/linux', root):
                files_set.add(os.path.join(root,filespath))

    return files_set

def main():
    """Entry point"""
    dir_files={}
    dir_cmds={}

    def has_parameter(arg):
        if not arg.startswith('-') and not arg.startswith('--'):
            raise CheckCopyrightError("Please figure out option.")
        else:
            return arg

    option = has_parameter(sys.argv[1])
    for arg in sys.argv[2:]:
        scanned_files=[]
        cmd_list=[]
        files_set = go_through_all_files(arg)
        for f in files_set:
            cmd = check_copyright(arg, f)
            if cmd:
                scanned_files.append(f)
                cmd_list.append(cmd)
        dir_files[arg]=scanned_files
        dir_cmds[arg]=cmd_list

    if option.find('s') != -1:
        for key, value in dir_files.viewitems():
            if key and value:
                print "Path: %s" % os.path.abspath(key)
                for f in value:
                    print "%s" % f
            else:
                print "Path: %s\nNo copyright in files needs to be "\
                    "updated." % os.path.abspath(key)
    elif option.find('f') != -1:
        for key, value in dir_cmds.viewitems():
            if key and value:
                print "Path: %s" % os.path.abspath(key)
                for cmd in value:
                    retcode = os.system(' '.join(cmd))
                    if retcode != 0:
                        raise CheckCopyrightError(
                            "Failed to update copyright in file '%s'." % file_path)
                    else:
                        print "Update %s done." % cmd[1]
            else:
                pass

if __name__ == '__main__':
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        print ('^C caught, program aborted.')
    except Exception:
        import traceback
        print(traceback.format_exc())
