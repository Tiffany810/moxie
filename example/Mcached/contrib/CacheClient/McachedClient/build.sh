#!/bin/bash
g++ -o mcachedclient *.cpp -std=c++11 -O3 -L/opt/brpc/lib/ -L/opt/leveldb/lib -L/opt/protobuf/lib -L/opt/openssl_1_0_2p/lib -L/opt/gflags/lib -L/opt/libmemcached/lib -lmemcached -lbrpc -lleveldb -lprotobuf -lgflags -lssl -lcrypto -ldl -lpthread -I/opt/brpc/include -I/opt/libmemcached/include -I/opt/protobuf/include -I./
