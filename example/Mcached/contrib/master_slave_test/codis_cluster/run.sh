#!/bin/bash
set -x
sleep 1
cd codis-master
./run.sh && cd ../
sleep 1
cd codis-slave-6377
./run.sh && cd ../
sleep 1
cd codis-slave-6378
./run.sh && cd ../
sleep 1
cd codis-sentinel
./run.sh && cd ../
sleep 1
rm -f ./zookeeper/bin/zookeeper.out
rm -f ./zookeeper.out
rm -rf /tmp/zookeeper
./zookeeper/bin/zkServer.sh start &
sleep 1
cd ./codis-proxy
./run.sh && cd ../
sleep 1
cd ./codis-dashboard
./run.sh && cd ../
sleep 5
cd ./codis-fe
./run.sh && cd ../
