#!/bin/sh

UT_DIR=./UT

if [ ! -d $UT_DIR ]
then
    echo "Cannot find UT directory, exit"
    exit 101
fi

if [ -z $1 ]
then
    echo "You must assign UT test group name"
    exit 101
fi

echo "generate test group: $1"

grep -r "TEST" $UT_DIR | grep $1 > UT_list.txt
sed -i 's/{//g' UT_list.txt

echo "UT_list is generated"
