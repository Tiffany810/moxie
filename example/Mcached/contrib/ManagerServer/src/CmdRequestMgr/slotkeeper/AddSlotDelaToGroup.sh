#!/bin/bash

set -x

if [ $# -ne 3 ]; then
    echo "Usage:AddSlotToGroup.sh slot_start slot_end Groupid"
    exit 0
fi

for((i=${1};i<=${2};i++));  
do
    curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":0, "source_id":'${3}', "dest_id":0, "slot_id":'${i}'}' http://127.0.0.1:8898/mcached/slotkeeper/
done 

echo ""
