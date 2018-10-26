#########################################################################
# File Name: killall.sh
# Author: fas
# Created Time: 2018年10月25日 星期四 21时39分39秒
#########################################################################
#!/bin/bash
set -x

ps -aux | grep codis- | grep -v grep | awk -F' ' '{print "kill -9 " $2}'|sh
ps -aux | grep redis-sentinel | grep -v grep | awk -F' ' '{print "kill -9 " $2}'|sh
./zookeeper/bin/zkServer.sh stop
