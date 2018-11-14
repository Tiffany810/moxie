#!/bin/bash
if [ $# -ne 2 ]
then
    echo "Usage:exec reqs datalen"
    exit 1
fi
./MemcachedClient -ip=192.168.56.1:11211 -rn 80 -reqs ${1} -datalen ${2}
