#include "trackers.h"

#include "absl/strings/str_format.h"
#include "bencode.h"
#include "network.h"

namespace ryu {
using namespace ::ryu::bencode;

namespace {
using net::IpAddress;
struct CompactIpv4Peer {
    uint32_t be_ipv4;
    uint16_t be_port;
    [[nodiscard]] PeerInfo ToPeerInfo() const {
        return {
            .ip = net::IpAddress::FromBe32(be_ipv4).ToString().TakeOrRaise(),
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
            .ip = net::IpAddress::FromBe128(be_ipv6).ToString().TakeOrRaise(),
            .port = be16toh(be_port),
        };
    }
} __attribute__((packed));
static_assert(sizeof(CompactIpv6Peer) == 18);
}  // namespace

Result<PeerInfo> PeerInfo::FromTrackerReply(const BencodeMap* map) {
    auto peer_id = TRY_OPTIONAL(
        map->try_string("peer id"),
        absl::StrCat("tracker replied peer dict missing peer id: ", map->json().TakeOrRaise()));
    auto ip = TRY_OPTIONAL(
        map->try_string("ip"),
        absl::StrCat("tracker replied peer dict missing ip: ", map->json().TakeOrRaise()));
    int64_t port = TRY_OPTIONAL(
        map->try_int("port"),
        absl::StrCat("tracker replied peer dict missing port: ", map->json().TakeOrRaise()));
    if (port < 0 || port >= 65536)
        return Err(absl::StrCat("tracker replied peer port out of range: ", port));
    return PeerInfo{
        .peer_id = peer_id,
        .ip = ip,
        .port = static_cast<uint16_t>(port),
    };
}

Result<std::vector<PeerInfo>> Trackers::ParsePeerInfoList(const BencodeMap& reply) {
    std::vector<PeerInfo> ret;

    // peers
    if (reply.try_object("peers") == nullptr) return Err("tracker reply missing peers");
    auto maybe_compat_peer_list = reply.try_string("peers");
    if (maybe_compat_peer_list) {
        // BEP-0023 compact IPv4 peer list
        if (maybe_compat_peer_list->size() % sizeof(CompactIpv4Peer) != 0)
            return Err(absl::StrFormat(
                "tracker replied compact list size incorrect: %u is not a multiple of %u",
                maybe_compat_peer_list->size(), sizeof(CompactIpv4Peer)));
        size_t length = maybe_compat_peer_list->size() / sizeof(CompactIpv4Peer);
        const auto* arr = reinterpret_cast<const CompactIpv4Peer*>(maybe_compat_peer_list->data());
        for (size_t i = 0; i < length; i++) ret.push_back(arr[i].ToPeerInfo());
    } else {
        // BEP-0003
        BencodeList* list = reply.try_object("peers")->try_list_object();
        if (list == nullptr) return Err("tracker replied peers is neither string nor list");
        for (size_t i = 0; i < list->size(); i++) {
            auto* map = TRY_POINTER(list->try_object(i)->try_map_object(),
                                    "tracker replied peer list contains non-map: " +
                                        list->try_object(i)->json().TakeOrRaise());
            ret.push_back(TRY(PeerInfo::FromTrackerReply(map)));
        }
    }

    // peers6, compact only, BEP-0007
    if (reply.try_object("peers6") != nullptr) {
        std::string peers6 = TRY_OPTIONAL(reply.try_string("peers6"),
                                          "tracker replied peers6 is not a string: " +
                                              reply.try_object("peers6")->json().TakeOrRaise());
        if (peers6.size() % sizeof(CompactIpv6Peer) != 0)
            return Err(absl::StrFormat(
                "tracker replied peers6 compact list size incorrect: %u is not a multiple of %u",
                peers6.size(), sizeof(CompactIpv6Peer)));
        size_t length = peers6.size() / sizeof(CompactIpv4Peer);
        const auto* arr = reinterpret_cast<const CompactIpv4Peer*>(peers6.data());
        for (size_t i = 0; i < length; i++) ret.push_back(arr[i].ToPeerInfo());
    }

    return ret;
}
Result<TrackerReply> Trackers::GetPeers(const std::string& announce, const std::string& info_hash,
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
    ASSIGN_OR_RAISE(auto reply_obj, BencodeObject::parse(rsp.text, &idx));
    auto reply = TRY_POINTER(reply_obj->try_map_object(),
                             "tracker reply is not an map: " + reply_obj->json().TakeOrRaise());

    // prepare return val
    TrackerReply ret;
    if (reply->try_object("failure reason")) {
        ret = {
            .failure_reason =
                TRY_OPTIONAL(reply->try_string("failure reason"),
                             "tracker replied failure reason is not string: " +
                                 reply->try_object("failure reason")->json().TakeOrRaise()),
        };
    } else {
        ret = {
            .interval = TRY_OPTIONAL(reply->try_int("interval"),
                                     "tracker reply doesn't contain valid interval"),
            .peers = TRY(ParsePeerInfoList(*reply)),
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