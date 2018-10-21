#!/bin/bash
g++ -o memcachedclient *.cpp -std=c++11 -L/opt/brpc/lib/ -L/opt/leveldb/lib -L/opt/protobuf/lib -L/opt/openssl_1_0_2p/lib -L/opt/gflags/lib -lbrpc -lleveldb -lprotobuf -lgflags -lssl -lcrypto -ldl -lpthread -I/opt/brpc/include -I/opt/protobuf/include -I./
