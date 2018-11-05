#!/bin/bash
cur_dir=`pwd`
#export mypath=$(cd ${cur_dir}; cd ../; pwd)
mypath=`cd ${cur_dir}; cd ../; pwd`
GOPATH=$GOPATH:${mypath}
go build -o ObserveServer
mkdir -p bin
mv ObserveServer ./bin/
