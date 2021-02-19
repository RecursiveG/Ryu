#include "torrent_file.h"

#include <fstream>
#include <string>

#include "absl/strings/str_cat.h"
#include "sha1.h"
using namespace std;

namespace ryu {
Result<TorrentFile, std::string> TorrentFile::Load(const std::string& bytes) {
    size_t idx = 0;
    ASSIGN_OR_RAISE(auto parsed_ptr, bencode::BencodeObject::Parse(bytes, &idx));
    const bencode::BencodeObject& parsed = *parsed_ptr;
    TorrentFile ret{};

    // announce
    auto maybe_announce = parsed["announce"].GetString();
    if (!maybe_announce) return Err("torrent missing announce url");
    ret.announce_ = *maybe_announce;

    auto ToStringVector =
        [](const bencode::BencodeList* blist) -> Result<std::vector<std::string>, std::string> {
        std::vector<std::string> ret;
        for (size_t j = 0; j < blist->Size(); j++) {
            auto maybe_str = (*blist)[j].GetString();
            if (!maybe_str)
                return Err("list element is not string: " + VALUE_OR_RAISE((*blist)[j].Json()));
            ret.push_back(std::move(*maybe_str));
        }
        return ret;
    };

    // announce-list
    if (parsed.Contains("announce-list")) {
        std::vector<std::vector<std::string>> groups;
        if (parsed["announce-list"].GetType() != bencode::Type::List) {
            return Err("announce-list is not a list");
        }
        const auto& announce_list = parsed["announce-list"];

        for (idx = 0; idx < announce_list.Size(); idx++) {
            if (announce_list[idx].GetType() != bencode::Type::List) {
                return Err("sub announce-list is not a list");
            }
            ASSIGN_OR_RAISE(
                auto group,
                ToStringVector(static_cast<const bencode::BencodeList*>(&announce_list[idx])));
            groups.push_back(group);
        }
        ret.alt_announce_list_ = groups;
    }

    // info
    if (!parsed.Contains("info")) return Err("torrent missing info");
    if (!parsed["info"].IsMap()) return Err("torrent info is not a map");
    const auto& info = parsed["info"];
    // info hash
    unsigned char info_hash_bytes[SHA1::HashBytes];
    std::string info_data = VALUE_OR_RAISE(info.Encode());
    SHA1 hasher{};
    hasher.add(info_data.c_str(), info_data.size());
    hasher.getHash(info_hash_bytes);
    ret.info_hash_ = string{reinterpret_cast<char*>(info_hash_bytes), SHA1::HashBytes};

    // info.piece length
    ret.piece_length_ =
        OPTIONAL_OR_RAISE(info["piece length"].GetInt(), "torrent info missing piece length");

    // info.pieces hash
    ret.hash_pool_ = OPTIONAL_OR_RAISE(info["pieces"].GetString(), "torrent info missing pieces");
    if ((ret.hash_pool_.size() % HASH_LENGTH) != 0)
        return Err("torrent info invalid pieces length");

    // info.torrent name
    ret.torrent_name_ = OPTIONAL_OR_RAISE(info["name"].GetString(), "torrent info missing name");

    // info.file list
    if (info["length"].IsInteger()) {
        // single file mode
        auto length = info["length"].GetInt().value();
        ret.files_.push_back(FileInfo{static_cast<uint64_t>(length), {ret.torrent_name_}});
        ret.total_length_ = length;
    } else {
        // multi file mode
        ret.total_length_ = 0;
        const auto& files = info["files"];
        if (!files.IsList()) return Err("torrent info missing files or length");

        for (size_t i = 0; i < files.Size(); i++) {
            size_t length = OPTIONAL_OR_RAISE(files[i]["length"].GetInt(),
                                              "torrent info files element missing length");
            const auto& path = files[i]["path"];
            if (!path.IsList()) return Err("torrent info files element path is not a list");

            ASSIGN_OR_RAISE(auto path_v,
                            ToStringVector(dynamic_cast<const bencode::BencodeList*>(&path)));
            path_v.insert(path_v.begin(), ret.torrent_name_);
            ret.files_.push_back(FileInfo{static_cast<uint64_t>(length), path_v});
            ret.total_length_ += length;
        }
    }
    if (ret.piece_length_ * ret.GetPieceCount() - ret.total_length_ >= ret.piece_length_)
        return Err("torrent piece count not match " +
                   absl::StrCat("total:", ret.total_length_, " piece_len:", ret.piece_length_,
                                " hash_len:", ret.hash_pool_.length(),
                                " piece_count:", ret.GetPieceCount()));
    ret.data_ = std::move(parsed_ptr);
    return ret;
}

Result<TorrentFile, std::string> TorrentFile::LoadFile(absl::string_view file_path) {
    ifstream ifile{string{file_path}, ios::binary};
    if (ifile) {
        std::string data{std::istreambuf_iterator<char>(ifile), std::istreambuf_iterator<char>{}};
        return TorrentFile::Load(data);
    } else {
        return Err(absl::StrCat("failed to open torrent file: ", file_path));
    }
}

}  // namespace ryu
