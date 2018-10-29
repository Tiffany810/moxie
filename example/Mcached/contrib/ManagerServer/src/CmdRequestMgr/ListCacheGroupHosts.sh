#!/bin/bash

if [ $# != 1 ]
then
    echo "Usage:ActivateGroup.sh groupid"
    exit 0
fi

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":7, "source_id":'${1}', "dest_id":0, "slot_id":0}' http://127.0.0.1:8898/Mcached/GroupRevise/
echo ""
