// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#include "floyd/src/floyd_impl.h"

#include <google/protobuf/text_format.h>

#include <utility>
#include <vector>
#include <algorithm>
#include <sys/eventfd.h>

#include "pink/include/bg_thread.h"
#include "slash/include/env.h"
#include "slash/include/slash_string.h"
#include "slash/include/slash_mutex.h"

#include "floyd/src/floyd_context.h"
#include "floyd/src/floyd_apply.h"
#include "floyd/src/floyd_worker.h"
#include "floyd/src/raft_log.h"
#include "floyd/src/floyd_peer_thread.h"
#include "floyd/src/floyd_primary_thread.h"
#include "floyd/src/floyd_client_pool.h"
#include "floyd/src/logger.h"
#include "floyd/src/floyd.pb.h"
#include "floyd/src/raft_meta.h"

namespace floyd {

static void BuildAddServerRequest(const std::string& new_server, CmdRequest* cmd) {
  cmd->set_type(Type::kAddServer);
  CmdRequest_AddServerRequest* add_server_request = cmd->mutable_add_server_request();
  add_server_request->set_new_server(new_server);
}

static void BuildRemoveServerRequest(const std::string& old_server, CmdRequest* cmd) {
  cmd->set_type(Type::kRemoveServer);
  CmdRequest_RemoveServerRequest* remove_server_request = cmd->mutable_remove_server_request();
  remove_server_request->set_old_server(old_server);
}

static void BuildGetAllServersRequest(CmdRequest* cmd) {
  cmd->set_type(Type::kGetAllServers);
}

static void BuildRaftTaskRequest(const RaftTask& task, CmdRequest* cmd) {
  assert(task.argv.size());
    cmd->set_type(Type::kRaftEntryTask);

  CmdRequest_RaftTaskRequest* raft_task_request = cmd->mutable_raft_task_request();
  for (size_t index = 0; index < task.argv.size(); ++index) {
    std::string* item = raft_task_request->add_args();
    *item = task.argv[index];
  }
  raft_task_request->set_reqid(task.reqid);
}

static void BuildRequestVoteResponse(uint64_t term, bool granted,
                                     CmdResponse* response) {
  response->set_type(Type::kRequestVote);
  CmdResponse_RequestVoteResponse* request_vote_res = response->mutable_request_vote_res();
  request_vote_res->set_term(term);
  request_vote_res->set_vote_granted(granted);
}

static void BuildAppendEntriesResponse(bool succ, uint64_t term,
                                       uint64_t log_index,
                                       CmdResponse* response) {
  response->set_type(Type::kAppendEntries);
  CmdResponse_AppendEntriesResponse* append_entries_res = response->mutable_append_entries_res();
  append_entries_res->set_term(term);
  append_entries_res->set_last_log_index(log_index);
  append_entries_res->set_success(succ);
}

static void BuildLogEntry(const CmdRequest& cmd, uint64_t current_term, Entry* entry) {
  entry->set_term(current_term);
  // for mcached
  if (cmd.type() == Type::kRaftEntryTask) {
    entry->set_optype(Entry_OpType_kRaftEntryTask);
    for (int i = 0; i < cmd.raft_task_request().args_size(); ++i) {
      *(entry->add_args()) = cmd.raft_task_request().args(i);
    }
    entry->set_reqid(cmd.raft_task_request().reqid());
  } else if (cmd.type() == Type::kAddServer) {
    entry->set_optype(Entry_OpType_kAddServer);
    entry->set_server(cmd.add_server_request().new_server());
  } else if (cmd.type() == Type::kRemoveServer) {
    entry->set_optype(Entry_OpType_kRemoveServer);
    entry->set_server(cmd.remove_server_request().old_server());
  } else if (cmd.type() == Type::kGetAllServers) {
    entry->set_optype(Entry_OpType_kGetAllServers);
  }
}

static void BuildMembership(const std::vector<std::string>& opt_members, Membership* members) {
  members->Clear();
  for (const auto& m : opt_members) {
    members->add_nodes(m);
  }
}

FloydImpl::FloydImpl(const Options& options)
  : wakeUpFd_(::eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC)),
    db_(NULL),
    raft_log_(NULL),
    raft_meta_(NULL),
    options_(options),
    info_log_(NULL),
    context_(NULL),
    worker_(NULL),
    apply_(NULL),
    primary_(NULL),
    worker_client_pool_(NULL){
    assert(this->wakeUpFd_ > 0);
}

FloydImpl::~FloydImpl() {
  // worker will use floyd, delete worker first
  if (worker_) {
    worker_->Stop();
    delete worker_;
  }

  if (primary_) {
    primary_->Stop();
    delete primary_;
  }
  
  if (apply_) {
    apply_->Stop();
    delete apply_;
  }
  
  if (worker_client_pool_) {
    delete worker_client_pool_;
  }
  
  for (auto& pt : peers_) {
    pt.second->Stop();
    delete pt.second;
  }

  if (context_) {
    delete context_;
  }

  if (raft_meta_) {
    delete raft_meta_;
  }

  if (raft_log_) {
    delete raft_log_;
  } 

  if (info_log_) {
    delete info_log_;
  }
  
  if (db_) {
    delete db_;
  }
  
}

bool FloydImpl::IsSelf(const std::string& ip_port) {
  return (ip_port == slash::IpPortString(options_.local_ip, options_.local_port));
}

bool FloydImpl::GetLeader(std::string *ip_port) {
  if (context_->leader_ip.empty() || context_->leader_port == 0) {
    return false;
  }
  *ip_port = slash::IpPortString(context_->leader_ip, context_->leader_port);
  return true;
}

bool FloydImpl::IsLeader() {
  if (context_->leader_ip == "" || context_->leader_port == 0) {
    return false;
  }
  if (context_->leader_ip == options_.local_ip && context_->leader_port == options_.local_port) {
    return true;
  }
  return false;
}

bool FloydImpl::IsSingle() {
  return options_.single_mode;
}

void FloydImpl::Process(const std::shared_ptr<moxie::PollerEvent>& event, moxie::EventLoop *loop) {
  ClearWake();
  RaftTask task;
  while (true) {
    {
      moxie::MutexLocker locker(taskMutex_);
      if (tasks_.empty()) {
        break;
      }
      task = tasks_.front();
      tasks_.pop();
    }
    this->DoRaftTask(task);
  }
}

void FloydImpl::SetApplyNotify(const std::function<bool (ApplyTask& apply)>& apply_notify) {
  moxie::MutexLocker locker(applyTaskMutex_);
  if (apply_notify) {
    apply_notify_ = apply_notify;
  }
}

void FloydImpl::DoRaftTask(RaftTask& task) {
  CmdRequest request;
  BuildRaftTaskRequest(task, &request);
  CmdResponse response;
  Status s = DoCommand(request, &response);
  if (!s.ok()) {
    ApplyTask apply;
    apply.reqid = task.reqid;
    apply.succeed = false;
    if (this->apply_notify_) {
        this->apply_notify_(apply);
    }
  }
}

int FloydImpl::GetWakeUpFd() const {
  return this->wakeUpFd_;
}

void FloydImpl::WakeUp() {
  uint64_t one = 1;
  ssize_t n = ::write(wakeUpFd_, &one, sizeof one);
  if (n != sizeof one) {
    LOGGER_ERROR("EventLoop::wakeup() writes " << n << " bytes instead of 8");
  }
}

void FloydImpl::PushTask(const RaftTask& task) {
  moxie::MutexLocker locker(taskMutex_);
  tasks_.emplace(task);
  WakeUp();
}

void FloydImpl::ClearWake() {
  uint64_t one = 1;
  ssize_t n = ::read(wakeUpFd_, &one, sizeof one);
  if (n != sizeof one){
    LOGGER_ERROR("EventLoop::handleRead() reads " << n << " bytes instead of 8");
  }
}

bool FloydImpl::GetLeader(std::string* ip, int* port) {
  *ip = context_->leader_ip;
  *port = context_->leader_port;
  return (!ip->empty() && *port != 0);
}

bool FloydImpl::HasLeader() {
  if (context_->leader_ip == "" || context_->leader_port == 0) {
    return false;
  }
  return true;
}

void FloydImpl::set_log_level(const int log_level) {
  if (info_log_) {
    info_log_->set_log_level(log_level);
  }
}

void FloydImpl::AddNewPeer(const std::string& server) {
  if (IsSelf(server)) {
    return;
  }
  // Add Peer
  auto peers_iter = peers_.find(server);
  if (peers_iter == peers_.end()) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ApplyAddMember server %s:%d add new peer thread %s",
        options_.local_ip.c_str(), options_.local_port, server.c_str());
    Peer* pt = new Peer(server, &peers_, context_, primary_, raft_meta_, raft_log_,
        worker_client_pool_, apply_, options_, info_log_);
    peers_.insert(std::pair<std::string, Peer*>(server, pt));
    pt->Start();
  }
}

void FloydImpl::RemoveOutPeer(const std::string& server) {
  if (IsSelf(server)) {
    return; 
  }
  // Stop and remove peer
  auto peers_iter = peers_.find(server);
  if (peers_iter != peers_.end()) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ApplyRemoveMember server %s:%d remove peer thread %s",
        options_.local_ip.c_str(), options_.local_port, server.c_str());
    peers_iter->second->Stop();
    delete peers_iter->second;
    peers_.erase(peers_iter);
  }
}

int FloydImpl::InitPeers() {
  // Create peer threads
  // peers_.clear();
  for (auto iter = context_->members.begin(); iter != context_->members.end(); iter++) {
    if (!IsSelf(*iter)) {
      Peer* pt = new Peer(*iter, &peers_, context_, primary_, raft_meta_, raft_log_,
          worker_client_pool_, apply_, options_, info_log_);
      peers_.insert(std::pair<std::string, Peer*>(*iter, pt));
    }
  }

  // Start peer thread
  int ret;
  for (auto& pt : peers_) {
    if ((ret = pt.second->Start()) != 0) {
      LOGV(ERROR_LEVEL, info_log_, "FloydImpl::InitPeers FloydImpl peer thread to %s failed to "
           " start, ret is %d", pt.first.c_str(), ret);
      return ret;
    }
  }
  LOGV(INFO_LEVEL, info_log_, "FloydImpl::InitPeers Floyd start %d peer thread", peers_.size());
  return 0;
}

Status FloydImpl::Init() {
  slash::CreatePath(options_.path);
  std::cout << "log_path:" << options_.path << std::endl;
  if (NewLogger(options_.path + "/LOG", &info_log_) != 0) {
    return Status::Corruption("Open LOG failed, ", strerror(errno));
  }

  // TODO(anan) set timeout and retry
  worker_client_pool_ = new ClientPool(info_log_);

  // Create DB
  rocksdb::Options options;
  options.create_if_missing = true;
  options.write_buffer_size = 1024 * 1024 * 1024;
  options.max_background_flushes = 8;
  rocksdb::Status s = rocksdb::DB::Open(options, options_.path + "/db/", &db_);
  if (!s.ok()) {
    LOGV(ERROR_LEVEL, info_log_, "Open db failed! path: %s", options_.path.c_str());
    return Status::Corruption("Open DB failed, " + s.ToString());
  }

  // Recover Context
  raft_log_ = new RaftLog(info_log_);
  raft_meta_ = new RaftMeta(info_log_);
  raft_meta_->Init();
  context_ = new FloydContext(options_);
  context_->RecoverInit(raft_meta_);

  // Recover Members when exist
  std::string mval;
  Membership db_members;
  s = db_->Get(rocksdb::ReadOptions(), kMemberConfigKey, &mval);
  if (s.ok()
      && db_members.ParseFromString(mval)) {
    // Prefer persistent membership than config
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::Init: Load Membership from db, count: %d", db_members.nodes_size());
    for (int i = 0; i < db_members.nodes_size(); i++) {
      context_->members.insert(db_members.nodes(i));
    }
  } else {
    BuildMembership(options_.members, &db_members);
    if(!db_members.SerializeToString(&mval)) {
      LOGV(ERROR_LEVEL, info_log_, "Serialize Membership failed!");
      return Status::Corruption("Serialize Membership failed");
    }
    s = db_->Put(rocksdb::WriteOptions(), kMemberConfigKey, mval);
    if (!s.ok()) {
      LOGV(ERROR_LEVEL, info_log_, "Record membership in db failed! error: %s", s.ToString().c_str());
      return Status::Corruption("Record membership in db failed! error: " + s.ToString());
    }
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::Init: Load Membership from option, count: %d", options_.members.size());
    for (const auto& m : options_.members) {
      context_->members.insert(m);
    }
  }

  // peers and primary refer to each other
  // Create PrimaryThread before Peers
  primary_ = new FloydPrimary(context_, &peers_, raft_meta_, options_, info_log_);

  // Start worker thread after Peers, because WorkerHandle will check peers
  worker_ = new FloydWorker(options_.local_port, 1000, this);
  int ret = 0;
  if ((ret = worker_->Start()) != 0) {
    LOGV(ERROR_LEVEL, info_log_, "FloydImpl::Init worker thread failed to start, ret is %d", ret);
    return Status::Corruption("failed to start worker, return " + std::to_string(ret));
  }
  // Apply thread should start at the last
  apply_ = new FloydApply(context_, db_, raft_meta_, raft_log_, this, info_log_);

  InitPeers();

  // Set and Start PrimaryThread
  // be careful:
  // primary and every peer threads shared the peer threads
  if ((ret = primary_->Start()) != 0) {
    LOGV(ERROR_LEVEL, info_log_, "FloydImpl::Init FloydImpl primary thread failed to start, ret is %d", ret);
    return Status::Corruption("failed to start primary thread, return " + std::to_string(ret));
  }
  primary_->AddTask(kCheckLeader);

  // we should start the apply thread at the last
  apply_->Start();
  // test only
  // options_.Dump();
  LOGV(INFO_LEVEL, info_log_, "FloydImpl::Init Floyd started!\nOptions\n%s", options_.ToString().c_str());
  return Status::OK();
}

Floyd::~Floyd() {
}

Status FloydImpl::AddServer(const std::string& new_server) {
  CmdRequest request;
  BuildAddServerRequest(new_server, &request);
  CmdResponse response;
  Status s = DoCommand(request, &response);
  if (!s.ok()) {
    return s;
  }
  if (response.code() == StatusCode::kOk) {
    return Status::OK();
  }

  return Status::Corruption("AddServer Error");
}

Status FloydImpl::RemoveServer(const std::string& old_server) {
  CmdRequest request;
  BuildRemoveServerRequest(old_server, &request);
  CmdResponse response;
  Status s = DoCommand(request, &response);
  if (!s.ok()) {
    return s;
  }
  if (response.code() == StatusCode::kOk) {
    return Status::OK();
  }

  return Status::Corruption("RemoveServer Error");
}

Status FloydImpl::GetAllServers(std::set<std::string>* nodes) {
  CmdRequest request;
  BuildGetAllServersRequest(&request);
  CmdResponse response;
  Status s = DoCommand(request, &response);
  if (!s.ok()) {
    return s;
  }
  if (response.code() == StatusCode::kOk) {
    nodes->clear();
    for (int i = 0; i < response.all_servers().nodes_size(); i++) {
      nodes->insert(response.all_servers().nodes(i));
    }
    return Status::OK();
  }
  return Status::Corruption("GetALlServers Error");
}


bool FloydImpl::GetServerStatus(std::string* msg) {
  LOGV(DEBUG_LEVEL, info_log_, "FloydImpl::GetServerStatus start");
  CmdResponse_ServerStatus server_status;
  {
  slash::MutexLock l(&context_->global_mu);
  DoGetServerStatus(&server_status);
  }

  char str[512];
  snprintf (str, sizeof(str),
            "      Node           |    Role    | Term |      Leader      |      VoteFor      | LastLogTerm | LastLogIdx | CommitIndex | LastApplied |\n"
            "%15s:%-6d%10s%7lu%14s:%-6d%14s:%-6d%10lu%13lu%14lu%13lu\n",
            options_.local_ip.c_str(), options_.local_port, server_status.role().c_str(), server_status.term(),
            server_status.leader_ip().c_str(), server_status.leader_port(),
            server_status.voted_for_ip().c_str(), server_status.voted_for_port(),
            server_status.last_log_term(), server_status.last_log_index(), server_status.commit_index(),
            server_status.last_applied());

  msg->clear();
  msg->append(str);
  return true;
}

Status FloydImpl::DoCommand(const CmdRequest& request, CmdResponse *response) { 
  // Execute if is leader
  std::string leader_ip;
  int leader_port;
  {
    slash::MutexLock l(&context_->global_mu);
    leader_ip = context_->leader_ip;
    leader_port = context_->leader_port;
  }
  if (options_.local_ip == leader_ip && options_.local_port == leader_port) {
    return DoCommandApply(request, response);
  } else if (leader_ip == "" || leader_port == 0) {
    return Status::Incomplete("no leader node!");
  }
  // Redirect to leader
  return worker_client_pool_->SendAndRecv(
      slash::IpPortString(leader_ip, leader_port),
      request, response);
}

bool FloydImpl::DoGetServerStatus(CmdResponse_ServerStatus* res) {
  std::string role_msg;
  switch (context_->role) {
    case Role::kFollower:
      role_msg = "follower";
      break;
    case Role::kCandidate:
      role_msg = "candidate";
      break;
    case Role::kLeader:
      role_msg = "leader";
      break;
  }

  res->set_term(context_->current_term);
  res->set_commit_index(context_->commit_index);
  res->set_role(role_msg);

  std::string ip;
  int port;
  ip = context_->leader_ip;
  port = context_->leader_port;
  if (ip.empty()) {
    res->set_leader_ip("null");
  } else {
    res->set_leader_ip(ip);
  }
  res->set_leader_port(port);

  ip = context_->voted_for_ip;
  port = context_->voted_for_port;
  if (ip.empty()) {
    res->set_voted_for_ip("null");
  } else {
    res->set_voted_for_ip(ip);
  }
  res->set_voted_for_port(port);

  uint64_t last_log_index;
  uint64_t last_log_term;
  raft_log_->GetLastLogTermAndIndex(&last_log_term, &last_log_index);

  res->set_last_log_term(last_log_term);
  res->set_last_log_index(last_log_index);
  res->set_last_applied(raft_meta_->GetLastApplied());
  return true;
}

Status FloydImpl::DoCommandApply(const CmdRequest& request,
                                 CmdResponse *response) {
  // Append entry local
  std::vector<const Entry*> entries;
  Entry entry;
  BuildLogEntry(request, context_->current_term, &entry);
  entries.push_back(&entry);
  response->set_type(request.type());
  response->set_code(StatusCode::kError);

  uint64_t last_log_index = raft_log_->Append(entries);
  if (last_log_index <= 0) {
    return Status::IOError("Append Entry failed");
  }

  // TODO:check can reply

  // Notify primary then wait for apply
  if (options_.single_mode) {
    context_->commit_index = last_log_index;
    raft_meta_->SetCommitIndex(context_->commit_index);
    apply_->ScheduleApply();
  } else {
    primary_->AddTask(kNewCommand);
  }
  return Status::OK();
}

rocksdb::Status FloydImpl::ApplyEntry(const Entry& entry) {
  rocksdb::Status ret;
    switch (entry.optype()) {
    case Entry_OpType_kRaftEntryTask:
      ret = this->ApplyRaftEntryTask(entry);
      break;
    case Entry_OpType_kAddServer:
      ret = MembershipChange(entry.server(), true);
      if (ret.ok()) {
        context_->members.insert(entry.server());
        this->AddNewPeer(entry.server());
      }
      LOGV(INFO_LEVEL, info_log_, "FloydApply::Apply Add server %s to cluster",
          entry.server().c_str());
      break;
    case Entry_OpType_kRemoveServer:
      ret = MembershipChange(entry.server(), false);
      if (ret.ok()) {
        context_->members.erase(entry.server());
        this->RemoveOutPeer(entry.server());
      }
      LOGV(INFO_LEVEL, info_log_, "FloydApply::Apply Remove server %s to cluster",
          entry.server().c_str());
      break;
    case Entry_OpType_kGetAllServers:
      ret = rocksdb::Status::OK();
      break;
    case Entry_OpType_kUnKnowCmd:
      ret = rocksdb::Status::Corruption("Unknown cmd type");
    default:
      ret = rocksdb::Status::Corruption("Unknown entry type");
  }
  return ret;
}

rocksdb::Status FloydImpl::ApplyRaftEntryTask(const Entry& entry) {
  if (entry.reqid() <= 0) {
    return rocksdb::Status::Corruption("Reqid invaild!");
  }
  ApplyTask task;
  task.reqid = entry.reqid();
  for (int i = 0; i < entry.args_size(); i++) {
    task.argv.push_back(entry.args()[i]);
  }
  task.succeed = true;
  if (apply_notify_) {
    apply_notify_(task);
  }
  return rocksdb::Status::OK();
}

rocksdb::Status FloydImpl::MembershipChange(const std::string& ip_port, bool add) {
  std::string value;
  Membership members;
  rocksdb::Status ret = db_->Get(rocksdb::ReadOptions(),
      kMemberConfigKey, &value);
  if (!ret.ok()) {
    return ret;
  }

  if(!members.ParseFromString(value)) {
    return rocksdb::Status::Corruption("Parse failed");
  }
  int count = members.nodes_size();
  for (int i = 0; i < count; i++) {
    if (members.nodes(i) == ip_port) {
      if (add) {
        return rocksdb::Status::OK();  // Already in
      }
      // Remove Server
      if (i != count - 1) {
        std::string *nptr = members.mutable_nodes(i);
        *nptr = members.nodes(count - 1);
      }
      members.mutable_nodes()->RemoveLast();
    }
  }

  if (add) {
    members.add_nodes(ip_port);
  }

  if (!members.SerializeToString(&value)) {
    return rocksdb::Status::Corruption("Serialize failed");
  }
  return db_->Put(rocksdb::WriteOptions(), kMemberConfigKey, value);
}

// Peer ask my vote with it's ip, port, log_term and log_index
void FloydImpl::GrantVote(uint64_t term, const std::string ip, int port) {
  // Got my vote
  context_->voted_for_ip = ip;
  context_->voted_for_port = port;
  context_->current_term = term;
}

int FloydImpl::ReplyRequestVote(const CmdRequest& request, CmdResponse* response) {
  slash::MutexLock l(&context_->global_mu);
  bool granted = false;
  CmdRequest_RequestVote request_vote = request.request_vote();
  LOGV(DEBUG_LEVEL, info_log_, "FloydImpl::ReplyRequestVote: my_term=%lu request.term=%lu",
       context_->current_term, request_vote.term());
  /*
   * If RPC request or response contains term T > currentTerm: set currentTerm = T, convert to follower (5.1)
   */
  if (request_vote.term() > context_->current_term) {
    context_->BecomeFollower(request_vote.term());
    raft_meta_->SetCurrentTerm(context_->current_term);
  }
  // if caller's term smaller than my term, then I will notice him
  if (request_vote.term() < context_->current_term) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyRequestVote: Leader %s:%d term %lu is smaller than my %s:%d current term %lu",
        request_vote.ip().c_str(), request_vote.port(), request_vote.term(), options_.local_ip.c_str(), options_.local_port,
        context_->current_term);
    BuildRequestVoteResponse(context_->current_term, granted, response);
    return -1;
  }
  uint64_t my_last_log_term = 0;
  uint64_t my_last_log_index = 0;
  raft_log_->GetLastLogTermAndIndex(&my_last_log_term, &my_last_log_index);
  // if votedfor is null or candidateId, and candidated's log is at least as up-to-date
  // as receiver's log, grant vote
  if ((request_vote.last_log_term() < my_last_log_term) ||
      ((request_vote.last_log_term() == my_last_log_term) && (request_vote.last_log_index() < my_last_log_index))) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyRequestVote: Leader %s:%d last_log_term %lu is smaller than my(%s:%d) last_log_term term %lu,"
        " or Leader's last log term equal to my last_log_term, but Leader's last_log_index %lu is smaller than my last_log_index %lu,"
        "my current_term is %lu",
        request_vote.ip().c_str(), request_vote.port(), request_vote.last_log_term(), options_.local_ip.c_str(), options_.local_port,
        my_last_log_term, request_vote.last_log_index(), my_last_log_index,context_->current_term);
    BuildRequestVoteResponse(context_->current_term, granted, response);
    return -1;
  }

  if (vote_for_.find(request_vote.term()) != vote_for_.end()
      && vote_for_[request_vote.term()] != std::make_pair(request_vote.ip(), request_vote.port())) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyRequestVote: I %s:%d have voted for %s:%d in this term %lu",
        options_.local_ip.c_str(), options_.local_port, vote_for_[request_vote.term()].first.c_str(), 
        vote_for_[request_vote.term()].second, request_vote.term());
    BuildRequestVoteResponse(context_->current_term, granted, response);
    return -1;
  }
  vote_for_[request_vote.term()] = std::make_pair(request_vote.ip(), request_vote.port());
  LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyRequestVote: Receive Request Vote from %s:%d, "
      "Become Follower with current_term_(%lu) and new_term(%lu)"
      " commit_index(%lu) last_applied(%lu)", request_vote.ip().c_str(), request_vote.port(),
      context_->current_term, request_vote.last_log_term(), my_last_log_index, context_->last_applied.load());
  context_->BecomeFollower(request_vote.term());
  raft_meta_->SetCurrentTerm(context_->current_term);
  raft_meta_->SetVotedForIp(context_->voted_for_ip);
  raft_meta_->SetVotedForPort(context_->voted_for_port);
  // Got my vote
  GrantVote(request_vote.term(), request_vote.ip(), request_vote.port());
  granted = true;
  LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyRequestVote: Grant my vote to %s:%d at term %lu",
      context_->voted_for_ip.c_str(), context_->voted_for_port, context_->current_term);
  context_->last_op_time = slash::NowMicros();
  BuildRequestVoteResponse(context_->current_term, granted, response);
  return 0;
}

bool FloydImpl::AdvanceFollowerCommitIndex(uint64_t leader_commit) {
  // Update log commit index
  /*
   * If leaderCommit > commitIndex, set commitIndex =
   *   min(leaderCommit, index of last new entry)
   */
  context_->commit_index = std::min(leader_commit, raft_log_->GetLastLogIndex());
  raft_meta_->SetCommitIndex(context_->commit_index);
  return true;
}

int FloydImpl::ReplyAppendEntries(const CmdRequest& request, CmdResponse* response) {
  bool success = false;
  CmdRequest_AppendEntries append_entries = request.append_entries();
  slash::MutexLock l(&context_->global_mu);
  // update last_op_time to avoid another leader election
  context_->last_op_time = slash::NowMicros();
  // Ignore stale term
  // if the append entries leader's term is smaller than my current term, then the caller must an older leader
  if (append_entries.term() < context_->current_term) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries: Leader %s:%d term %lu is smaller than my %s:%d current term %lu",
        append_entries.ip().c_str(), append_entries.port(), append_entries.term(), options_.local_ip.c_str(), options_.local_port,
        context_->current_term);
    BuildAppendEntriesResponse(success, context_->current_term, raft_log_->GetLastLogIndex(), response);
    return -1;
  } else if ((append_entries.term() > context_->current_term) 
      || (append_entries.term() == context_->current_term && 
        (context_->role == kCandidate || (context_->role == kFollower && context_->leader_ip == "")))) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries: Leader %s:%d term %lu is larger than my %s:%d current term %lu, "
        "or leader term is equal to my current term, my role is %d, leader is [%s:%d]",
        append_entries.ip().c_str(), append_entries.port(), append_entries.term(), options_.local_ip.c_str(), options_.local_port,
        context_->current_term, context_->role, context_->leader_ip.c_str(), context_->leader_port);
    context_->BecomeFollower(append_entries.term(),
        append_entries.ip(), append_entries.port());
    raft_meta_->SetCurrentTerm(context_->current_term);
    raft_meta_->SetVotedForIp(context_->voted_for_ip);
    raft_meta_->SetVotedForPort(context_->voted_for_port);
  }

  if (append_entries.prev_log_index() > raft_log_->GetLastLogIndex()) {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries: Leader %s:%d prev_log_index %lu is larger than my %s:%d last_log_index %lu",
        append_entries.ip().c_str(), append_entries.port(), append_entries.prev_log_index(), options_.local_ip.c_str(), options_.local_port,
        raft_log_->GetLastLogIndex());
    BuildAppendEntriesResponse(success, context_->current_term, raft_log_->GetLastLogIndex(), response);
    return -1;
  }

  // Append entry
  if (append_entries.prev_log_index() < raft_log_->GetLastLogIndex()) {
    LOGV(WARN_LEVEL, info_log_, "FloydImpl::ReplyAppendEtries: Leader %s:%d prev_log_index(%lu, %lu) is smaller than"
        " my last_log_index %lu, truncate suffix from %lu", append_entries.ip().c_str(), append_entries.port(),
        append_entries.prev_log_term(), append_entries.prev_log_index(), raft_log_->GetLastLogIndex(),
        append_entries.prev_log_index() + 1);
    raft_log_->TruncateSuffix(append_entries.prev_log_index() + 1);
  }

  // we compare peer's prev index and term with my last log index and term
  uint64_t my_last_log_term = 0;
  Entry entry;
  LOGV(DEBUG_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries "
      "prev_log_index: %lu\n", append_entries.prev_log_index());
  if (append_entries.prev_log_index() == 0) {
    my_last_log_term = 0;
  } else if (raft_log_->GetEntry(append_entries.prev_log_index(), &entry) == 0) {
    my_last_log_term = entry.term();
  } else {
    LOGV(WARN_LEVEL, info_log_, "FloydImple::ReplyAppentries: can't "
        "get Entry from raft_log prev_log_index %llu", append_entries.prev_log_index());
    BuildAppendEntriesResponse(success, context_->current_term, raft_log_->GetLastLogIndex(), response);
    return -1;
  }

  if (append_entries.prev_log_term() != my_last_log_term) {
    LOGV(WARN_LEVEL, info_log_, "FloydImpl::ReplyAppentries: leader %s:%d pre_log(%lu, %lu)'s term don't match with"
         " my log(%lu, %lu) term, truncate my log from %lu", append_entries.ip().c_str(), append_entries.port(),
         append_entries.prev_log_term(), append_entries.prev_log_index(), my_last_log_term, raft_log_->GetLastLogIndex(),
         append_entries.prev_log_index());
    // TruncateSuffix [prev_log_index, last_log_index)
    raft_log_->TruncateSuffix(append_entries.prev_log_index());
    BuildAppendEntriesResponse(success, context_->current_term, raft_log_->GetLastLogIndex(), response);
    return -1;
  }

  std::vector<const Entry*> entries;
  for (int i = 0; i < append_entries.entries().size(); i++) {
    entries.push_back(&append_entries.entries(i));
  }
  if (append_entries.entries().size() > 0) {
    LOGV(DEBUG_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries: Leader %s:%d will append %u entries from "
         " prev_log_index %lu", append_entries.ip().c_str(), append_entries.port(),
         append_entries.entries().size(), append_entries.prev_log_index());
    if (raft_log_->Append(entries) <= 0) {
      LOGV(ERROR_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries: Leader %s:%d ppend %u entries from "
          " prev_log_index %lu error at term %lu", append_entries.ip().c_str(), append_entries.port(),
          append_entries.entries().size(), append_entries.prev_log_index(), append_entries.term());
      BuildAppendEntriesResponse(success, context_->current_term, raft_log_->GetLastLogIndex(), response);
      return -1;
    }
  } else {
    LOGV(INFO_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries: Receive PingPong AppendEntries from %s:%d at term %lu",
        append_entries.ip().c_str(), append_entries.port(), append_entries.term());
  }
  if (append_entries.leader_commit() != context_->commit_index) {
    AdvanceFollowerCommitIndex(append_entries.leader_commit());
    apply_->ScheduleApply();
  }
  success = true;
  // only when follower successfully do appendentries, we will update commit index
  LOGV(DEBUG_LEVEL, info_log_, "FloydImpl::ReplyAppendEntries server %s:%d Apply %d entries from Leader %s:%d"
      " prev_log_index %lu, leader commit %lu at term %lu", options_.local_ip.c_str(),
      options_.local_port, append_entries.entries().size(), append_entries.ip().c_str(),
      append_entries.port(), append_entries.prev_log_index(), append_entries.leader_commit(),
      append_entries.term());
  BuildAppendEntriesResponse(success, context_->current_term, raft_log_->GetLastLogIndex(), response);
  return 0;
}

}  // namespace floyd
