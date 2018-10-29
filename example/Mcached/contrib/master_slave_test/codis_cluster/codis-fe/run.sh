#########################################################################
# File Name: run.sh
# Author: fas
# Created Time: 2018年10月26日 星期五 10时02分34秒
#########################################################################
#!/bin/bash
./codis-admin --dashboard-list --zookeeper=127.0.0.1:2181 > codis.json
./codis-fe --ncpu=1 --log=./fe.log --log-level=WARN --dashboard-list=./codis.json --listen=0.0.0.0:8090 &
