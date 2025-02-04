// Copyright 2023, DragonflyDB authors.  All rights reserved.
// See LICENSE for licensing terms.
//

#pragma once

#include "base/logging.h"
#include "core/fibers.h"
#include "facade/reply_capture.h"
#include "server/conn_context.h"

namespace dfly {

// MultiCommandSquasher allows executing a series of commands under a multi transaction
// and squashing multiple consecutive single-shard commands into one hop whenever it's possible,
// thus greatly decreasing the dispatch overhead for them.
class MultiCommandSquasher {
 public:
  static void Execute(absl::Span<StoredCmd> cmds, ConnectionContext* cntx,
                      bool error_abort = false) {
    MultiCommandSquasher{cmds, cntx, error_abort}.Run();
  }

 private:
  // Per-shard exection info.
  struct ShardExecInfo {
    ShardExecInfo() : had_writes{false}, cmds{}, replies{}, local_tx{nullptr} {
    }

    bool had_writes;
    std::vector<StoredCmd*> cmds;  // accumulated commands
    std::vector<facade::CapturingReplyBuilder::Payload> replies;
    boost::intrusive_ptr<Transaction> local_tx;  // stub-mode tx for use inside shard
  };

  enum class SquashResult { SQUASHED, SQUASHED_FULL, NOT_SQUASHED, ERROR };

  static constexpr int kMaxSquashing = 32;

 private:
  MultiCommandSquasher(absl::Span<StoredCmd> cmds, ConnectionContext* cntx, bool error_abort);

  // Lazy initialize shard info.
  ShardExecInfo& PrepareShardInfo(ShardId sid);

  // Retrun squash flags
  SquashResult TrySquash(StoredCmd* cmd);

  // Execute separate non-squashed cmd.
  void ExecuteStandalone(StoredCmd* cmd);

  // Callback that runs on shards during squashed hop.
  facade::OpStatus SquashedHopCb(Transaction* parent_tx, EngineShard* es);

  // Execute all currently squashed commands. Return true if aborting on error.
  bool ExecuteSquashed();

  // Run all commands until completion.
  void Run();

 private:
  absl::Span<StoredCmd> cmds_;  // Input range of stored commands
  ConnectionContext* cntx_;     // Underlying context
  const CommandId* base_cid_;   // either EVAL or EXEC, used for squashed hops

  bool error_abort_ = false;  // Abort upon receiving error

  std::vector<ShardExecInfo> sharded_;
  std::vector<ShardId> order_;  // reply order for squashed cmds

  // multi modes that lock on hops (non-atomic, incremental) need keys for squashed hops.
  // track_keys_ stores whether to populate collected_keys_
  bool track_keys_;
  absl::flat_hash_set<MutableSlice> collected_keys_;

  std::vector<MutableSlice> tmp_keylist_;
};

}  // namespace dfly
