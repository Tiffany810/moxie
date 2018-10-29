#########################################################################
# File Name: ReqService.sh
# Author: fas
# Created Time: 2018年10月23日 星期二 21时29分58秒
#########################################################################
#!/bin/bash

if [ $# -ne 2 ] 
then
    echo "Usage:DelSlotToHost.sh ip:port slot_id"
    echo "Example:DelSlotToHost.sh 127.0.0.1:13579 2"
    exit 0
fi

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":1, "group_id":0, "slot_id":'${2}'}' http://127.0.0.1:13579
echo ""
