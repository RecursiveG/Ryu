#include "trackers.h"

#include "absl/strings/str_format.h"
#include "common/bencode.h"
#include "common/network.h"

namespace ryu {
using namespace ::ryu::bencode;

namespace {
using net::IpAddress;
struct CompactIpv4Peer {
    uint32_t be_ipv4;
    uint16_t be_port;
    [[nodiscard]] PeerInfo ToPeerInfo() const {
        return {
            .ip = net::IpAddress::FromBe32(be_ipv4).ToString().Expect("Failed to convert IPv4"),
            .port = be16toh(be_port),
        };
    }
} __attribute__((packed));
static_assert(sizeof(CompactIpv4Peer) == 6);

struct CompactIpv6Peer {
    uint8_t be_ipv6[16];
    uint16_t be_port;
    [[nodiscard]] PeerInfo ToPeerInfo() const {
        return {
            .ip = net::IpAddress::FromBe128(be_ipv6).ToString().Expect("Failed to convert IPv6"),
            .port = be16toh(be_port),
        };
    }
} __attribute__((packed));
static_assert(sizeof(CompactIpv6Peer) == 18);
}  // namespace

Result<PeerInfo, std::string> PeerInfo::FromTrackerReply(const BencodeMap* map_ptr) {
    const BencodeMap& map = *map_ptr;

    auto peer_id = OPTIONAL_OR_RAISE(
        map["peer id"].GetString(),
        absl::StrCat("tracker replied peer dict missing peer id: ", map.Json().Expect("")));
    auto ip = OPTIONAL_OR_RAISE(
        map["ip"].GetString(),
        absl::StrCat("tracker replied peer dict missing ip: ", map.Json().Expect("")));
    int64_t port = OPTIONAL_OR_RAISE(
        map["port"].GetInt(),
        absl::StrCat("tracker replied peer dict missing port: ", map.Json().Expect("")));
    if (port < 0 || port >= 65536)
        return Err(absl::StrCat("tracker replied peer port out of range: ", port));
    return PeerInfo{
        .peer_id = peer_id,
        .ip = ip,
        .port = static_cast<uint16_t>(port),
    };
}

Result<std::vector<PeerInfo>, std::string> Trackers::ParsePeerInfoList(const BencodeMap& reply) {
    std::vector<PeerInfo> ret;

    // peers
    if (!reply.Contains("peers")) return Err("tracker reply missing peers");
    if (reply["peers"].IsString()) {
        // BEP-0023 compact IPv4 peer list
        std::string peer_list = reply["peers"].GetString().value();

        if (peer_list.size() % sizeof(CompactIpv4Peer) != 0)
            return Err(absl::StrFormat(
                "tracker replied compact list size incorrect: %u is not a multiple of %u",
                peer_list.size(), sizeof(CompactIpv4Peer)));
        size_t length = peer_list.size() / sizeof(CompactIpv4Peer);
        const auto* arr = reinterpret_cast<const CompactIpv4Peer*>(peer_list.data());
        for (size_t i = 0; i < length; i++) ret.push_back(arr[i].ToPeerInfo());
    } else if (reply["peers"].IsList()) {
        // BEP-0003
        const BencodeList* list = dynamic_cast<const BencodeList*>(&reply["peers"]);
        if (list == nullptr) return Err("tracker replied peers is neither string nor list");
        for (size_t i = 0; i < list->Size(); i++) {
            const auto& map = list[i];
            if (!map.IsMap()) {
                return Err("tracker replied peer list contains non-map: " +
                           VALUE_OR_RAISE(map.Json()));
            }
            const auto* map_ptr = dynamic_cast<const BencodeMap*>(&map);
            ret.push_back(VALUE_OR_RAISE(PeerInfo::FromTrackerReply(map_ptr)));
        }
    } else {
        return Err(absl::StrCat("tracker reply peers unexpected value: ",
                                VALUE_OR_RAISE(reply["peers"].Json())));
    }

    // peers6, compact only, BEP-0007
    const std::optional<std::string> peers6 = reply["peers6"].GetString();
    if (peers6) {
        if (peers6->size() % sizeof(CompactIpv6Peer) != 0)
            return Err(absl::StrFormat(
                "tracker replied peers6 compact list size incorrect: %u is not a multiple of %u",
                peers6->size(), sizeof(CompactIpv6Peer)));
        size_t length = peers6->size() / sizeof(CompactIpv4Peer);
        const auto* arr = reinterpret_cast<const CompactIpv4Peer*>(peers6->data());
        for (size_t i = 0; i < length; i++) ret.push_back(arr[i].ToPeerInfo());
    }

    return ret;
}
Result<TrackerReply, std::string> Trackers::GetPeers(const std::string& announce,
                                                     const std::string& info_hash,
                                                     uint64_t left_bytes) {
    if (info_hash.size() != 20) return Err("invalid info_hash");
    cpr::Response rsp =
        cpr::Get(cpr::Url{announce}, cpr::Parameters{{"info_hash", info_hash},
                                                     {"peer_id", "-RY0000-0123456789ab"},
                                                     {"port", "6881"},
                                                     {"uplaoded", "0"},
                                                     {"downloaded", "0"},
                                                     {"left", absl::StrCat(left_bytes)},
                                                     {"event", "started"}});
    // check http result
    if (rsp.error.code != cpr::ErrorCode::OK) {
        return Err(
            absl::StrCat("GET request failed tracker=", announce, " msg=", rsp.error.message));
    }
    if (rsp.status_code != 200) {
        return Err(absl::StrCat("GET request failed tracker=", announce,
                                " status_code=", rsp.status_code));
    }
    // parse return payload
    size_t idx = 0;
    ASSIGN_OR_RAISE(auto reply_obj, BencodeObject::Parse(rsp.text, &idx));
    if (!reply_obj->IsMap()) {
        return Err("tracker reply is not an map: " + VALUE_OR_RAISE(reply_obj->Json()));
    }
    const auto& reply = *reply_obj.get();

    // prepare return val
    TrackerReply ret;
    if (reply.Contains("failure reason")) {
        ret = {
            .failure_reason = OPTIONAL_OR_RAISE(reply["failure reason"].GetString(),
                                                "tracker replied failure reason is not string: " +
                                                    VALUE_OR_RAISE(reply["failure reason"].Json())),
        };
    } else {
        ret = {
            .interval = OPTIONAL_OR_RAISE(reply["interval"].GetInt(),
                                          "tracker reply doesn't contain valid interval"),
            .peers = VALUE_OR_RAISE(ParsePeerInfoList(dynamic_cast<const BencodeMap&>(reply))),
        };
    }

    // cleanup tracker
    rsp = cpr::Get(cpr::Url{announce}, cpr::Parameters{{"info_hash", info_hash},
                                                       {"peer_id", "-RY0000-0123456789ab"},
                                                       {"port", "6881"},
                                                       {"uplaoded", "0"},
                                                       {"downloaded", "0"},
                                                       {"left", absl::StrCat(left_bytes)},
                                                       {"event", "stopped"}});

    // return
    return ret;
}
}  // namespace ryu