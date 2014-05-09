#!/bin/sh

TEST_LIST=test_list.txt
LINE_CACHE=
TEST_TIME=`date +%Y%m%d%H%M%S`
TEST_NAME=
TEST_CLASS=
TEST_REPORT=test_report_$TEST_TIME
TEST_BIN=libcamhal_test

if [ -e $TEST_LIST ]
then
    echo "We have $TEST_LIST, start to run each test"

    mkdir -p $TEST_REPORT
else
    echo "Cannot fine $TEST_LIST, exit"
    exit 101
fi


if [ ! -e $TEST_BIN ]
then
    echo "case test tool is not exist"
    exit 101
fi

test_func(){
    local THIS_CASE_TEST=$1
    local THIS_REPORT=$2

    ./$TEST_BIN --gtest_filter="$THIS_CASE_TEST" > $THIS_REPORT &
    local THIS_TEST_PID=`pgrep $TEST_BIN`
    echo "waiting libcamhal_test $THIS_TEST_PID"

    wait $THIS_TEST_PID
}


cat $TEST_LIST | while read LINE_CACHE
do
    TEST_CLASS=`echo $LINE_CACHE | awk '{print $1}' | cut -d'(' -f 2 | sed 's/,//g'`
    TEST_NAME=`echo $LINE_CACHE | awk '{print $2}' | sed 's/)//g'`
    SKIP=`echo $LINE_CACHE | awk '{print $3}'`

    CASE_TEST="$TEST_CLASS.$TEST_NAME"
    echo $CASE_TEST

    if [ "$SKIP"x = "SKIP"x ]
    then
        echo "!!! Skip this case $CASE_TEST"
        continue
    fi

    REPORT_FILE="$TEST_REPORT/$CASE_TEST.log"
    touch $REPORT_FILE

    test_func $CASE_TEST $REPORT_FILE

    sleep 1
done

# Print Report #

FAILED_NUM=`find $TEST_REPORT -type f -exec grep -Hn "FAILED TEST" {} \; | wc -l`
SUCCEED_NUM=`find $TEST_REPORT -type f -exec grep -Hn "PASSED" {} \; | wc -l`
TOTAL_NUM=`ls -l $TEST_REPORT | grep "^-" | wc -l`

echo "Test summary: failed tests: $FAILED_NUM out of total tests: $TOTAL_NUM"
echo "Failed tests:"
find $TEST_REPORT -type f -exec grep -Hn "FAILED TEST" {} \;
echo " "

LOG_LIST=log_list

rm -f $LOG_LIST
touch $LOG_LIST
ls $TEST_REPORT > $LOG_LIST

cat $LOG_LIST | while read LINE_CACHE
do
    grep -Hn "PASSED" $TEST_REPORT/$LINE_CACHE > /dev/null 2>&1
    RET1=`echo $?`

    if [ $RET1 -eq 1 ]
    then
        grep -Hn "FAILED TEST" $TEST_REPORT/$LINE_CACHE > /dev/null 2>&1
        RET2=`echo $?`

        if [ $RET2 -eq 1 ]
        then
            echo "This case goes weird, no PASSED, no FAILED TEST: $TEST_REPORT/$LINE_CACHE"
        fi
    fi
done

rm -f $LOG_LIST

