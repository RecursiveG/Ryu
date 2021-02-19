#ifndef RYU_TRACKERS_H
#define RYU_TRACKERS_H

#include <cpr/cpr.h>

#include <cinttypes>
#include <string>
#include <vector>

#include "common/bencode.h"
#include "result.h"

namespace ryu {

struct PeerInfo {
    std::string peer_id{};
    std::string ip{};
    uint16_t port{};
    static Result<PeerInfo, std::string> FromTrackerReply(const bencode::BencodeMap* map);
};

struct TrackerReply {
    std::string failure_reason{};
    int64_t interval{};
    std::vector<PeerInfo> peers{};
};

class Trackers {
  public:
    static Result<std::vector<PeerInfo>, std::string> ParsePeerInfoList(const bencode::BencodeMap& reply);
    static Result<TrackerReply, std::string> GetPeers(const std::string& announce, const std::string& info_hash,
                                         uint64_t left_bytes);
};
}  // namespace ryu

#endif  // RYU_TRACKERS_H
