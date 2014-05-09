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

set -ue
set -o pipefail

# The path to the libcustomized_3a repository root.
srcdir=$(readlink -f "$(dirname $0)/../")

# Directory in which to place RPM (defaults to $srcdir).
destdir="${1:-${srcdir}}"

# Cleanup tempdir on exit or interrupt.
tempdir=$(mktemp -d)
trap "rm -rf ${tempdir}" SIGINT SIGTERM EXIT

# Create directories needed by rpmbuild.
mkdir -p ${tempdir}/rpmbuild/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

# Used if _RPM_RELEASE is not defined.
latest_change_id=$(git log --no-merges -n1 | awk -F ':' '/Change-Id:/ {print $2}' | cut -c 37-43)
latest_commit_id=$(git log -n1 | awk -F ' ' '/commit/ {print $2}' | cut -c 1-7)

# The RPM version and release tags can be overriden from the envrionment
rpm_version=${_RPM_VERSION:-1.0.0}
rpm_release=${_RPM_RELEASE:-$(date "+%Y%m%d%H%M.")${latest_commit_id}}

rpmbuild --define "_buildshell /bin/bash"  \
         --define "_topdir   ${tempdir}/rpmbuild" \
         --define "_srcdir   ${srcdir}" \
         --define "_tmppath  ${tempdir}/tmp" \
         --define "version   ${rpm_version}" \
         --define "release   ${rpm_release}" \
         -bb "$(dirname $0)"/libcustomized_3a.spec | tee "$tempdir/rpmbuild.log"

# Copy rpm to source directory.
rpm_path=$(awk '/^Wrote:.*rpm$/ { print $2; exit }' "$tempdir/rpmbuild.log")
cp -v "$rpm_path" "$destdir/rpm"
