#!/bin/bash

if [ $# != 1 ]
then
    echo "Usage:CacheKeepAlive.sh groupid"
    exit 0
fi

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":0, "is_master":true, "hosts":"127.0.0.1:8899", "source_id":'${1}'}' http://127.0.0.1:8898/Mcached/EndPoints/
echo ""
