#########################################################################
# File Name: run.sh
# Author: fas
# Created Time: 2018年10月23日 星期二 20时55分02秒
#########################################################################
#!/bin/bash
rm -rf infra1.etcd
./etcd --name infra1 --listen-client-urls http://127.0.0.1:32379 --advertise-client-urls http://127.0.0.1:32379 --listen-peer-urls http://127.0.0.1:12380 --initial-advertise-peer-urls http://127.0.0.1:12380
