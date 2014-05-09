#!/bin/bash

check_dir_exist() {
  if [ ! -d $1 ]; then
    echo "---------directory: $1 not exist, please report to maintainer---------"
    exit 1
  fi
}

check_file_exist() {
  if [ ! -f $1 ]; then
    echo "---------file: $1 not exist, please report to maintainer---------"
    exit 1
  fi
}

check_str_exist() {
  if [ -z $2 ]; then
    echo "---------string: $1 not exist, please report to maintainer---------"
    exit 1
  fi
}

# acquire username of origin builder
check_file_exist "chroot/etc/profile.env"
portage_username=`cat chroot/etc/profile.env  | grep "PORTAGE_USERNAME" | cut -d " " -f 2`
src_username=`echo $portage_username | sed -nr "/^PORTAGE_USERNAME=/s/.*='(.*)'/\1/p"`
# acquire uid and username in this environment
sys_uid=`id -u`
sys_username=`getent passwd "$sys_uid" | cut -d: -f1`
check_str_exist "src_username" $src_username
check_str_exist "sys_username" $sys_username
echo "############## current uid: $sys_uid, username: $sys_username ############"

echo "############## update permissions and username in config files ############"
# change folder name with current username to access to chrome trunk
check_dir_exist "chroot/home"
pushd chroot/home
check_dir_exist "$src_username"
sudo mv $src_username $sys_username

# update folder's ownership and group
sudo chown -R $sys_username $sys_username
sudo chgrp -R $sys_username $sys_username

# update username in config files
check_dir_exist "$sys_username/.vpython-root"
pushd $sys_username/.vpython-root
for dir in ./*/
do
  pushd $dir/bin
  sudo sed -i "s/$src_username/$sys_username/g" `sudo grep $src_username -rl *`
  popd
done

popd
popd

check_dir_exist "chroot/build/cnlrvp"
pushd chroot/build/cnlrvp
check_file_exist "var/cache/portage/sys-kernel/chromeos-kernel-be-cnl/include/generated/compile.h"
sudo sed -i "s/$src_username/$sys_username/g" var/cache/portage/sys-kernel/chromeos-kernel-be-cnl/include/generated/compile.h
check_file_exist "usr/src/chromeos-kernel-be-cnl-9999/build/include/generated/compile.h"
sudo sed -i "s/$src_username/$sys_username/g" usr/src/chromeos-kernel-be-cnl-9999/build/include/generated/compile.h
popd

echo "############## update local manifest ##############"
# re-setup links between xml files
check_dir_exist ".repo"
pushd .repo/
rm local_manifest.xml
check_file_exist "camera_setup/local_manifest.xml-camera-chrome-cnl"
ln -s camera_setup/local_manifest.xml-camera-chrome-cnl local_manifest.xml
popd

echo "############## update uid number when log into chrome OS ##############"
# change uid number in chroot/etc/passwd and chroot/etc/group
check_dir_exist "chroot/etc"
pushd chroot/etc
check_file_exist "passwd"
sudo sed -i "s/$src_username/$sys_username/g" `sudo grep $src_username -rl *`
line_number=`sudo cat passwd | grep -n "ChromeOS Developer" | cut -d ":" -f 1`
sudo sed -i $line_number'd' passwd
sudo sed -i "$line_number i $sys_username:x:$sys_uid:$sys_uid:ChromeOS Developer:/home/$sys_username:/bin/bash" passwd

check_file_exist "group"
line_number=`sudo cat group | grep -n "$sys_username:x" | cut -d ":" -f 1`
sudo sed -i $line_number'd' group
sudo sed -i "$line_number i $sys_username:x:$sys_uid:$sys_username" group
popd

echo "############# system reconfiguration done ##############"
