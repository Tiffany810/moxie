#!/bin/bash

if [ $# != 1 ]
then
    echo "Usage:NameResolver.sh server_name"
    exit 0
fi

curl -H "Content-Type:application/json" -X POST --data '{"server_name":"'${1}'"}' http://127.0.0.1:8898/mcached/nameresolver/
echo ""
