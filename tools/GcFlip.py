#
#  Copyright (C) 2018 Intel Corporation
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

#!/usr/bin/python

"""
 \Brief this is a tool which changes the metadata value of
  dp/mp/ppp output port to make mirror or flip feature
"""
import os.path
import sys, getopt

NO_FLIP = "0,2,0,0"
V_FLIP = "0,2,1,0"
H_FLIP = "0,2,2,0"
H_V_FLIP = "0,2,3,0"
GC_FILE = "/usr/share/defaults/etc/camera/gcss/graph_descriptor.xml"
OUTPUT_PORTS = ("bxt_ofa_mp", "bxt_ofa_dp", "bxt_ofa_ppp")

def usage():
    print "Error input parameters:", str(sys.argv[1:])
    print("Please input '-v/-h-n'(--vflip/--hflip/--none) for flip")

def get_user_input():
    try:
        opts, args = getopt.getopt(sys.argv[1:], "hvn", ["hflip", "vflip", "none"])
    except getopt.GetoptError:
        usage()
        return False, False

    vflip = False
    hflip = False
    no_flip = False
    for option, value in opts:
        if option in ("-h", "--hflip"):
            hflip = True
            print("Modify graph config for hflip")
        if option in ("-v", "--vflip"):
            vflip = True
            print("Modify graph config for vflip")
        if option in ("-n", "--none"):
            no_flip = True
            print("Modify graph config for normal")

    return vflip, hflip, no_flip


# Find the metadata position and replace it's value
def replace_data(line, vflip, hflip, no_flip):
    for port in OUTPUT_PORTS:
        if port in line:
            # Find the position of metadata value
            start_ops = line.find("metadata")
            start_ops = line.find("=", start_ops)
            end_ops = line.find(" ", start_ops)
            metadata = line[start_ops + 2 : end_ops - 1]
            if vflip and hflip:
                line = line.replace(metadata, H_V_FLIP)
            elif vflip:
                line = line.replace(metadata, V_FLIP)
            elif hflip:
                line = line.replace(metadata, H_FLIP)
            elif no_flip:
                line = line.replace(metadata, NO_FLIP)

    return line

def graph_config_for_flip():
    if not GC_FILE:
        return

    if not os.path.exists(GC_FILE):
        print('Invalid graph config file:', GC_FILE)
        return

    vflip, hflip, no_flip = get_user_input()
    if not vflip and not hflip and not no_flip:
        usage()
        return

    with open(GC_FILE, "r") as f:
        lines = f.readlines()

    with open(GC_FILE, "w") as f_w:
        for line in lines:
            if "kernel" in line:
                line = replace_data(line, vflip, hflip, no_flip)
            f_w.write(line)

if __name__ == '__main__':
    graph_config_for_flip()
