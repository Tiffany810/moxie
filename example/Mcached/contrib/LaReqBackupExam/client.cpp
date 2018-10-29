// Copyright (c) 2014 Baidu, Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//     http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// A client sending requests to server every 1 second.

#include <gflags/gflags.h>
#include <butil/logging.h>
#include <butil/time.h>
#include <brpc/channel.h>
#include "echo.pb.h"

DEFINE_string(attachment, "foo", "Carry this along with requests");
DEFINE_string(protocol, "baidu_std", "Protocol type. Defined in src/brpc/options.proto");
DEFINE_string(connection_type, "pooled", "Connection type. Available values: single, pooled, short");
DEFINE_string(server, "list://192.168.56.1:8000,192.168.56.1:8001,192.168.56.1:8002", "IP Address of server");
DEFINE_string(load_balancer, "rr", "The algorithm for load balancing");
DEFINE_int32(timeout_ms, 100, "RPC timeout in milliseconds");
DEFINE_int32(max_retry, 3, "Max retries(not including the first RPC)"); 
DEFINE_int32(interval_ms, 1000, "Milliseconds between consecutive requests");
DEFINE_string(http_content_type, "application/json", "Content type of http request");

static inline uint64_t time_now(void) {
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return ((uint64_t) time.tv_sec) * 1000000000 + time.tv_nsec;
}

int main(int argc, char* argv[]) {
    // Parse gflags. We recommend you to use gflags as well.
    GFLAGS_NS::ParseCommandLineFlags(&argc, &argv, true);
    
    brpc::ChannelOptions options;
    options.protocol = FLAGS_protocol;
    options.connection_type = FLAGS_connection_type;
    options.timeout_ms = FLAGS_timeout_ms/*milliseconds*/;
    options.max_retry = FLAGS_max_retry;
    
    std::vector<uint32_t> reqs = {1000, 2000, 3000, 4000, 5000,};
    std::vector<std::string> balance = {"la", "rr", "random"};

    for (size_t i = 0; i < balance.size(); i++) {
        for (size_t j = 0; j < reqs.size(); ++j) {
            brpc::Channel channel;
            if (channel.Init(FLAGS_server.c_str(), 
                        balance[i].c_str(), 
                        &options) != 0) {
                LOG(ERROR) << "Fail to initialize channel";
                return -1;
            }
            example::EchoService_Stub stub(&channel);
            uint64_t start = time_now() / 1000;
            for (size_t k = 0; k < reqs[j]; ++k) {
                example::EchoRequest request;
                example::EchoResponse response;
                brpc::Controller cntl;
                request.set_message("hello world");
                if (FLAGS_protocol != "http" && FLAGS_protocol != "h2c")  {
                    cntl.request_attachment().append(FLAGS_attachment);
                } else {
                    cntl.http_request().set_content_type(FLAGS_http_content_type);
                }
                stub.Echo(&cntl, &request, &response, NULL);
            }
            uint64_t end = time_now() / 1000;
            LOG(INFO) << "balancer:" << balance[i] << " reqs:" << reqs[j] << " total_time:" << end - start << std::endl;
        }
        std::cout << std::endl << std::endl; 
    }

    LOG(INFO) << "EchoClient is going to quit";
    return 0;
}
