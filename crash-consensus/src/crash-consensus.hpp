#pragma once

#include <functional>
#include <memory>
#include <vector>

namespace dory {
class RdmaConsensus;

enum class ProposeError {
  NoError = 0,  // Placeholder for the 0 value
  MutexUnavailable,
  FastPath,
  FastPathRecyclingTriggered,
  SlowPathCatchFUO,
  SlowPathUpdateFollowers,
  SlowPathCatchProposal,
  SlowPathUpdateProposal,
  SlowPathReadRemoteLogs,
  SlowPathWriteAdoptedValue,
  SlowPathWriteNewValue,
  FollowerMode,
  SlowPathLogRecycled
};

class Consensus {
 public:
  Consensus(int my_id, std::vector<int> &remote_ids);
  ~Consensus();

  void commitHandler(
      std::function<void(bool leader, uint8_t *buf, size_t len)> committer);

  ProposeError propose(uint8_t *buf, size_t len);
  int potentialLeader();

 private:
  std::unique_ptr<RdmaConsensus> impl;
};
}  // namespace dory
