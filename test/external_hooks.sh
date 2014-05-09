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
echo $1 $2 $XRCHost
if [ ! -f /tmp/imageWidth ]; then
    touch /tmp/imageWidth
fi
if [ ! -f /tmp/imageHeight ]; then
    touch /tmp/imageHeight
fi
if [ ! -f /tmp/imageFormat ]; then
    touch /tmp/imageFormat
fi
if [ ! -f /tmp/interlacedFlag ]; then
    touch /tmp/interlacedFlag
fi
if [ ! -f /tmp/flagPollBuffer ]; then
    touch /tmp/flagPollBuffer
fi
if [ ! -f /tmp/flagPollStopped ]; then
    touch /tmp/flagPollStopped
fi
if [ ! -f /tmp/flagPollStarted ]; then
    touch /tmp/flagPollStarted
fi
imageWidth=`cat /tmp/imageWidth`
imageHeight=`cat /tmp/imageHeight`
imageFormat=`cat /tmp/imageFormat`
interlacedFlag=`cat /tmp/interlacedFlag`
flagPollBuffer=`cat /tmp/flagPollBuffer`
flagPollStopped=`cat /tmp/flagPollStopped`
flagPollStarted=`cat /tmp/flagPollStarted`

case $1 in
    interlaced)
        if [ -z $2 ];then
            echo "Error: Invalid interlaced data: $1 $2"
            unset interlacedFlag
            echo "" > /tmp/interlacedFlag
        else
            interlacedFlag=$2
            echo $interlacedFlag > /tmp/interlacedFlag
        fi
        ;;
    WHF)
        arr=(${2//,/ })
        if [ ${#arr[@]} -ne 3 ];then
            unset imageWidth
            echo "" > /tmp/imageWidth
            unset imageHeight
            echo "" > /tmp/imageHeight
            unset imageFormat
            echo "" > /tmp/imageFormat
            echo "Error: Invalid WHF data: $1 $2"
        else
            imageWidth=${arr[0]}
            echo $imageWidth > /tmp/imageWidth
            imageHeight=${arr[1]}
            echo $imageHeight > /tmp/imageHeight
            imageFormat=${arr[2]}
            echo $imageFormat > /tmp/imageFormat
        fi
        ;;
    flag)
        if [[ -n $flagPollStarted && -z $flagPollBuffer && $2 = "poll_buffer" ]];then
            flagPollBuffer=$2
            echo $flagPollBuffer > /tmp/flagPollBuffer
            echo "*** Invoke data feeding: interlaced $interlacedFlag, format $imageFormat, width $imageWidth, height $imageHeight ***"
            #sleep 2s
            if [[ $imageHeight = "1080" && $interlacedFlag = "0" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-1080p-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-1080p-free-run.py" > /dev/udp/${XRCHost}/13579
            elif [[ $imageHeight = "720" && $interlacedFlag = "0" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-720p-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-720p-free-run.py" > /dev/udp/${XRCHost}/13579
            elif [[ $imageHeight = "576" && $interlacedFlag = "0" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-576p-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-576p-free-run.py" > /dev/udp/${XRCHost}/13579
            elif [[ $imageHeight = "480" && $interlacedFlag = "0" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-VGA-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-VGA-free-run.py" > /dev/udp/${XRCHost}/13579
            elif [[ $imageHeight = "540" && $interlacedFlag = "1" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-1080i-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-1080i-free-run.py" > /dev/udp/${XRCHost}/13579
            elif [[ $imageHeight = "288" && $interlacedFlag = "1" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-576i-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-576i-free-run.py" > /dev/udp/${XRCHost}/13579
            elif [[ $imageHeight = "240" && $interlacedFlag = "1" ]];then
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-480i-free-run.py" > /dev/udp/${XRCHost}/13579
                echo "01_22_HDMI_to_MIPI_TxA_CSI_4_Lane_YUV422_8_Bit_Over_600Mbps -mondello-480i-free-run.py" > /dev/udp/${XRCHost}/13579
            else
                echo "Error: No matching XRC script found."
            fi
        elif [[ $2 = "poll_buffer" ]];then
            echo "duplicated poll buffer, ignore"
        elif [[ $2 = "poll_started" ]];then
            flagPollStarted=$2
            echo $flagPollStarted > /tmp/flagPollStarted
        elif [[ $2 = "poll_stopped" ]];then
            flagPollStopped=$2
            echo $flagPollStopped > /tmp/flagPollStopped
            unset flagPollBuffer
            echo "" > /tmp/flagPollBuffer
            unset flagPollStarted
            echo "" > /tmp/flagPollStarted
        else
            unset flagPollBuffer
            echo "" > /tmp/flagPollBuffer
            unset flagPollStopped
            echo "" > /tmp/flagPollStopped
            unset flagPollStarted
            echo "" > /tmp/flagPollStarted
            echo "Error: Invalid flag: $1 $2"
        fi
        ;;
    *)
        echo "Error: Invalid input parameters: $1 $2"
        ;;
esac
