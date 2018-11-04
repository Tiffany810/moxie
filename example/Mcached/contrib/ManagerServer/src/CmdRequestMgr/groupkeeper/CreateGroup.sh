#!/bin/bash

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":0, "group_id":0, "ext":""}' http://127.0.0.1:8898/mcached/groupkeeper/
echo ""
