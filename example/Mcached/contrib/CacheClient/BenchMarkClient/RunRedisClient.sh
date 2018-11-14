#!/bin/bash
if [ $# -ne 2 ]
then
    echo "Usage:exec reqs datalen"
    exit 1
fi
./RedisClient -ip=192.168.56.1:6379 -rn 100 -reqs ${1} -datalen ${2}
