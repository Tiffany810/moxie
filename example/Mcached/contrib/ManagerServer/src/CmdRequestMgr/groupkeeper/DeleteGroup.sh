#!/bin/bash

if [ $# -ne 1 ]
then
    echo "Usage:DeleteGroup.sh groupid"
    exit 0
fi

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":2, "group_id":'${1}', "ext":""}' http://127.0.0.1:8898/mcached/groupkeeper/
echo ""
