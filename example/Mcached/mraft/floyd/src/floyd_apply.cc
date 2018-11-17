// Copyright (c) 2015-present, Qihoo, Inc.  All rights reserved.
// This source code is licensed under the BSD-style license found in the
// LICENSE file in the root directory of this source tree. An additional grant
// of patent rights can be found in the PATENTS file in the same directory.

#include "floyd/src/floyd_apply.h"

#include <google/protobuf/text_format.h>

#include <unistd.h>
#include <string>

#include "slash/include/xdebug.h"
#include "slash/include/env.h"

#include "floyd/src/logger.h"
#include "rocksdb/db.h"
#include "floyd/src/floyd.pb.h"
#include "floyd/src/raft_meta.h"
#include "floyd/src/raft_log.h"
#include "floyd/src/floyd_impl.h"

namespace floyd {

FloydApply::FloydApply(FloydContext* context, rocksdb::DB* db, RaftMeta* raft_meta,
    RaftLog* raft_log, FloydImpl* impl, Logger* info_log)
  : bg_thread_(1024 * 1024 * 1024),
    context_(context),
    db_(db),
    raft_meta_(raft_meta),
    raft_log_(raft_log),
    impl_(impl),
    info_log_(info_log) {
}

FloydApply::~FloydApply() {
}

int FloydApply::Start() {
  bg_thread_.set_thread_name("A:" + std::to_string(impl_->GetLocalPort()));
  bg_thread_.Schedule(ApplyStateMachineWrapper, this);
  return bg_thread_.StartThread();
}

int FloydApply::Stop() {
  return bg_thread_.StopThread();
}

void FloydApply::ScheduleApply() {
  /*
   * int timer_queue_size, queue_size;
   * bg_thread_.QueueSize(&timer_queue_size, &queue_size);
   * LOGV(INFO_LEVEL, info_log_, "Peer::AddRequestVoteTask timer_queue size %d queue_size %d",
   *     timer_queue_size, queue_size);
   */
  bg_thread_.Schedule(&ApplyStateMachineWrapper, this);
}

void FloydApply::ApplyStateMachineWrapper(void* arg) {
  reinterpret_cast<FloydApply*>(arg)->ApplyStateMachine();
}

void FloydApply::ApplyStateMachine() {
  uint64_t last_applied = context_->last_applied;
  // Apply as more entry as possible
  uint64_t commit_index;
  commit_index = context_->commit_index;

  LOGV(DEBUG_LEVEL, info_log_, "FloydApply::ApplyStateMachine: last_applied: %lu, commit_index: %lu",
            last_applied, commit_index);
  Entry log_entry;
  if (last_applied >= commit_index) {
    return;
  }
  // TODO: use batch commit to optimization
  while (last_applied < commit_index) {
    last_applied++;
    raft_log_->GetEntry(last_applied, &log_entry);
    // TODO: we need change the s type
    // since the Apply may not operate rocksdb
    rocksdb::Status s = Apply(log_entry);
    if (!s.ok()) {
      LOGV(WARN_LEVEL, info_log_, "FloydApply::ApplyStateMachine: Apply log entry failed, at: %d, error: %s",
          last_applied, s.ToString().c_str());
      usleep(1000000);
      ScheduleApply();  // try once more
      return;
    }
  }
  context_->apply_mu.Lock();
  context_->last_applied = last_applied;
  raft_meta_->SetLastApplied(last_applied);
  context_->apply_mu.Unlock();
  context_->apply_cond.SignalAll();
}

rocksdb::Status FloydApply::Apply(const Entry& entry) {
  // be careful:
  // we need to return the ret carefully
  // the FloydApply::ApplyStateMachine need use the ret to judge
  // whether consume this successfully
  return impl_->ApplyEntry(entry);
}

} // namespace floyd
