#!/bin/bash
rm -f ./*.rdb
./redis-sentinel ./sentinel.conf
