#pragma once

namespace dory {
namespace ConsensusConfig {
static constexpr bool pinThreads = true;

// If pinThreads is true, make sure to set the core id appropriately.
// You can use `numatcl -H` to see which cores each numa domain has.
// In hyperthreaded CPUs, numactl first lists the non-hyperthreaded cores
// and then the hyperthreaded ones.

static constexpr int consensusThreadCoreID = 0;
static constexpr int switcherThreadCoreID = 2;
static constexpr int heartbeatThreadCoreID = 4;

static constexpr bool nameThreads = true;
static const char consensusThreadName[] = "thd_consensus";
static const char switcherThreadName[] = "thd_switcher";
static const char heartbeatThreadName[] = "thd_heartbeat";

}  // namespace ConsensusConfig
}  // namespace dory