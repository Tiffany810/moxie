#!/bin/bash

if [ $# != 2 ]
then
    echo "Usage:ServerKeepAlive.sh groupid server_name"
    exit 0
fi

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":0, "is_master":true, "hosts":"127.0.0.1:8899", "source_id":'${1}', "server_name":"'${2}'"}' http://127.0.0.1:8898/mcached/keepalive/
echo ""
