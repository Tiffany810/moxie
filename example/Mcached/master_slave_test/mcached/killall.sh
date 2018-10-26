#########################################################################
# File Name: killall.sh
# Author: fas
# Created Time: 2018年10月26日 星期五 13时07分28秒
#########################################################################
#!/bin/bash
set -x
ps -aux | grep Mcached | grep -v grep | awk -F' ' '{print "kill -9 "$2}' | sh
