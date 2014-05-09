#
#  Copyright (C) 2014-2016 Intel Corporation
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

#!/bin/bash

########################################################
# This tool is used to reduce confilct HW usage.
#
#  Install board_run.sh
#    # cp board_run.sh /usr/local/bin/
#    # chmod a+x board_run.sh
#
#  Run with board_run.sh
#    # board_run.sh <your command>
#
#  Note: board_run.sh will use the current directory as
#        user, so it is better to run board_run.sh under
#        a meaningful name, like ~/hyzhu/.
########################################################

LOCK_FILE=/var/tmp/bxt_run_lock
can_run=no

trap '{>$LOCK_FILE; exit1;}' INT

if [ -f $LOCK_FILE ] ; then
    touch $LOCK_FILE
fi

#try lock
while true; do
  cur_user=`cat $LOCK_FILE`

  if [ -z "$cur_user" ] ; then
    user="$PWD"
    echo $user >$LOCK_FILE
    break;
  else
    echo "$cur_user" is using this board, please wait for a while.
  fi

  sleep 1;
done

#run command
$*

#release lock
>$LOCK_FILE
