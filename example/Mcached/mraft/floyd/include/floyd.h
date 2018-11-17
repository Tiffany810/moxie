// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#ifndef FLOYD_INCLUDE_FLOYD_H_
#define FLOYD_INCLUDE_FLOYD_H_

#include <string>
#include <set>

#include "floyd/include/floyd_options.h"
#include "slash/include/slash_status.h"

namespace floyd {

using slash::Status;

class Floyd  {
 public:
  Floyd() { }
  virtual ~Floyd();

  // membership change interface
  virtual Status AddServer(const std::string& new_server) = 0;
  virtual Status RemoveServer(const std::string& out_server) = 0;

  // return true if leader has been elected
  virtual bool GetLeader(std::string* ip_port) = 0;
  virtual bool GetLeader(std::string* ip, int* port) = 0;
  virtual bool HasLeader() = 0;
  virtual bool IsLeader() = 0;
  virtual bool IsSingle() = 0;
  virtual Status GetAllServers(std::set<std::string>* nodes) = 0;

  // used for debug
  virtual bool GetServerStatus(std::string* msg) = 0;

  // log level can be modified
  virtual void set_log_level(const int log_level) = 0;

  virtual Status ExecMcached(const std::vector<std::string>& args, std::string& res) = 0;
 private:
  // No coping allowed
  Floyd(const Floyd&);
  void operator=(const Floyd&);
};

} // namespace floyd
#endif  // FLOYD_INCLUDE_FLOYD_H_
