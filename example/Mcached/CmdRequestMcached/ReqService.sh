#########################################################################
# File Name: ReqService.sh
# Author: fas
# Created Time: 2018年10月23日 星期二 21时29分58秒
#########################################################################
#!/bin/bash
curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":0, "source_id":0, "dest_id":0, "slot_id":0}' http://127.0.0.1:13579
