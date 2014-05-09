#!/bin/bash
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

get_str(){
    ext=$1
    y_time=$2

    for suffix in h cpp cc c java json txt
    do
        if [ "$ext" = $suffix -a ! -z "y_time" ]; then
            str="\ * Copyright (C) $y_time Intel Corporation"
        fi
    done

    for suffix in Makefile py am sh mk README
    do
        if [ "$ext" = $suffix -a ! -z "y_time" ]; then
            str="#  Copyright (C) $y_time Intel Corporation"
        fi
    done

    if [ "$ext" = "xml" -a -n "y_time" ];then
        str="\  Copyright (C) $y_time Intel Corporation"
    fi

    echo "$str"
}

file_path=$1
cr_template=$2
y_time=$3
flag_insert=$4
flag_update=$5

auth=`stat -c "%a" $file_path`
file_name=$(echo ${file_path##*/})

if [ "$file_name" = "Makefile" ]; then
    ext=$file_name
else
    ext=${file_name##*.}
fi

insert_str=$(get_str $ext $y_time)

if [ -e "$file_path" ]; then
    if [ -n "$y_time" ];then
            ##### insert intel copyright into existed copyrights
        if [ "$flag_insert" = "true" ]; then
            line_num=`awk '/Copyright/{print NR}' $file_path`
            last=${line_num:((${#line_num} - 1))}
            insert_line=$(expr $last + 1)
            sed -i "$insert_line i $insert_str" $file_path
            ##### update time for intel copyright
        elif [ "$flag_update" = "true" ]; then
            x=`grep "Copyright" $file_path`
            y=`echo "$x" | grep "Intel" | grep -o "[0-9]\{4\}.*\s" |  cut -d' ' -f 1 | cut -d'-' -f 1`
            z="$y-$y_time"
            if [[ "$ext" = "py" || "$ext" = "sh" || "$ext" = "mk" || "$ext" = "Makefile" || "$ext" = "am" || "$ext" = "README" ]]; then
                sed -i "s/#\s*Copyright.*Corp.*$/#  Copyright (C) $z Intel Corporation/g" $file_path
            elif [ "$ext" != "xml" ]; then
                sed -i "s/ * Copyright.*Corp.*$/ Copyright (C) $z Intel Corporation/g" $file_path
            else
                sed -i "s/\s*Copyright.*Corp.*$/  Copyright (C) $z Intel Corporation/g" $file_path
            fi
            #### add copyright section to file
        else
            if [ -e "$cr_template" ]; then
                if [[ "$ext" = "py" || "$ext" = "sh" ]]; then
                    sed -i "1 r $cr_template" $file_path
                    sed -i "3 i $insert_str" $file_path
                else
                    cat $cr_template $file_path > $file_path.tmp
                    sed -i "2 i $insert_str" $file_path.tmp
                    mv $file_path.tmp $file_path
                    sudo chmod $auth $file_path
                fi
            else
                echo "No correct copyright template to use."
                exit 1
            fi
       fi
   fi
else
    echo "No file passed to update copyright."
    exit 1
fi
