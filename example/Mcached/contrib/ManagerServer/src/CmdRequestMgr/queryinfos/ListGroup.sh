#!/bin/bash

if [ $# -ne 2 ]; then
    echo "Usage:ListGroup.sh page_size page_index"
    exit -1
fi

curl -H "Content-Type:application/json" -X POST --data '{"cmd_type":1, "request":"{\"page_size\":'${1}', \"page_index\":'${2}'}"}' http://127.0.0.1:8898/mcached/queryinfos/
echo ""
