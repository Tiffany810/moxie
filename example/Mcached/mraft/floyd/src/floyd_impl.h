// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#ifndef FLOYD_SRC_FLOYD_IMPL_H_
#define FLOYD_SRC_FLOYD_IMPL_H_

#include <string>
#include <set>
#include <utility>
#include <memory>
#include <map>

#include "slash/include/slash_mutex.h"
#include "slash/include/slash_status.h"
#include "pink/include/bg_thread.h"
#include "rocksdb/db.h"
#include "third/moxie/base/EventLoop.h"
#include "third/moxie/base/Thread.h"
#include "third/moxie/base/Handler.h"
#include "third/moxie/base/PollerEvent.h"
#include "third/moxie/base/Mutex.h"
#include "third/moxie/base/MutexLocker.h"

#include "floyd/include/floyd.h"
#include "floyd/include/floyd_options.h"
#include "floyd/src/raft_log.h"
#include "floyd/src/raft_task.h"

namespace floyd {
using slash::Status;

class Log;
class ClientPool;
class RaftMeta;
class Peer;
class FloydPrimary;
class FloydApply;
class FloydWorker;
class FloydWorkerConn;
class FloydContext;
class Logger;
class CmdRequest;
class CmdResponse;
class CmdResponse_ServerStatus;

typedef std::map<std::string, Peer*> PeersSet;

static const std::string kMemberConfigKey = "#MEMBERCONFIG";

class FloydImpl : public Floyd, public moxie::Handler, public std::enable_shared_from_this<FloydImpl> {
 public:
  explicit FloydImpl(const Options& options);
  virtual ~FloydImpl();

  Status Init();

  // membership change interface
  virtual Status AddServer(const std::string& new_server) override;
  virtual Status RemoveServer(const std::string& out_server) override;
  virtual Status GetAllServers(std::set<std::string>* nodes) override;

  // return true if leader has been elected
  virtual bool GetLeader(std::string* ip_port) override;
  virtual bool GetLeader(std::string* ip, int* port) override;
  virtual bool HasLeader() override;
  virtual bool IsLeader() override;
  virtual bool IsSingle() override;

  virtual void Process(const std::shared_ptr<moxie::PollerEvent>& event, moxie::EventLoop *loop);
  int GetWakeUpFd() const;
  void ClearWake();
  void WakeUp();
  void PushTask(const RaftTask& task);
  void DoRaftTask(RaftTask& task);
  void SetApplyNotify(const std::function<bool (ApplyTask& apply)>& apply_notify);

  int GetLocalPort() {
    return options_.local_port;
  }

  virtual bool GetServerStatus(std::string* msg);
  // log level can be modified
  void set_log_level(const int log_level);
  // used when membership changed
  void AddNewPeer(const std::string& server);
  void RemoveOutPeer(const std::string& server);
  rocksdb::Status ApplyEntry(const Entry& entry);
 private:
  // friend class Floyd;
  friend class FloydWorkerConn;
  friend class FloydWorkerHandle;
  friend class Peer;

  int wakeUpFd_;
  std::queue<RaftTask> tasks_;
  moxie::Mutex taskMutex_;
  moxie::Mutex applyTaskMutex_;
  std::function<bool (ApplyTask& apply)> apply_notify_;

  rocksdb::DB* db_;
  // state machine db point
  // raft log
  RaftLog* raft_log_;
  RaftMeta* raft_meta_;

  Options options_;
  Logger* info_log_;

  FloydContext* context_;

  FloydWorker* worker_;
  FloydApply* apply_;
  FloydPrimary* primary_;
  PeersSet peers_;
  ClientPool* worker_client_pool_;

  std::map<int64_t, std::pair<std::string, int> > vote_for_;

  bool IsSelf(const std::string& ip_port);

  Status DoCommand(const CmdRequest& cmd, CmdResponse *cmd_res);
  Status DoCommandApply(const CmdRequest& cmd, CmdResponse *cmd_res);
  rocksdb::Status ApplyRaftEntryTask(const Entry& entry);
  rocksdb::Status MembershipChange(const std::string& ip_port, bool add);
  bool DoGetServerStatus(CmdResponse_ServerStatus* res);
  void GrantVote(uint64_t term, const std::string ip, int port);

  /*
   * these two are the response to the request vote and appendentries
   */
  int ReplyRequestVote(const CmdRequest& cmd, CmdResponse* cmd_res);
  int ReplyAppendEntries(const CmdRequest& cmd, CmdResponse* cmd_res);

  bool AdvanceFollowerCommitIndex(uint64_t new_commit_index);

  int InitPeers();

  // No coping allowed
  FloydImpl(const FloydImpl&);
  void operator=(const FloydImpl&);
};  // FloydImpl

}  // namespace floyd
#endif  // FLOYD_SRC_FLOYD_IMPL_H_
