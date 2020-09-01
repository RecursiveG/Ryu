#include "torrent_file.h"

#include <fstream>

#include "absl/strings/str_cat.h"
using namespace std;

namespace ryu {
Result<TorrentFile> TorrentFile::Load(const std::string& bytes) {
    size_t idx = 0;
    ASSIGN_OR_RAISE(auto parsed, bencode::BencodeObject::parse(bytes, &idx));
    TorrentFile ret{};

    auto maybe_announce = parsed->try_string("announce");
    if (!maybe_announce) return Err("torrent missing announce url");
    ret.announce_ = *maybe_announce;

    auto ToStringVector = [](bencode::BencodeList* blist) -> Result<std::vector<std::string>> {
        std::vector<std::string> ret;
        for (size_t j = 0; j < blist->size(); j++) {
            auto maybe_str = blist->try_string(j);
            if (!maybe_str)
                return Err("list element is not string: " +
                           blist->try_object(j)->json().TakeOrRaise());
            ret.push_back(std::move(*maybe_str));
        }
        return ret;
    };

    if (parsed->try_object("announce-list")) {
        std::vector<std::vector<std::string>> groups;
        auto* announce_list = parsed->try_object("announce-list")->try_list_object();
        if (nullptr == announce_list) return Err("announce-list is not a list");
        for (idx = 0; idx < announce_list->size(); idx++) {
            auto* sublist = announce_list->try_object(idx)->try_list_object();
            if (nullptr == sublist) return Err("sub announce-list is not a list");
            ASSIGN_OR_RAISE(auto group, ToStringVector(sublist));
            groups.push_back(group);
        }
        ret.alt_announce_list_ = groups;
    }

    if (parsed->try_object("info") == nullptr) return Err("torrent missing info");
    auto* info = parsed->try_object("info")->try_map_object();
    if (info == nullptr) return Err("torrent info is not a map");

    auto maybe_piece_length = info->try_int("piece length");
    if (!maybe_piece_length) return Err("torrent info missing piece length");
    ret.piece_length_ = *maybe_piece_length;

    auto maybe_hash_pool = info->try_string("pieces");
    if (!maybe_hash_pool) return Err("torrent info missing pieces");
    ret.hash_pool_ = *maybe_hash_pool;
    if ((ret.hash_pool_.size() % HASH_LENGTH) != 0)
        return Err("torrent info invalid pieces length");

    auto maybe_name = info->try_string("name");
    if (!maybe_name) return Err("torrent info missing name");
    ret.torrent_name_ = *maybe_name;

    if (info->try_int("length")) {
        // single file mode
        auto length = *info->try_int("length");
        ret.files_.push_back(FileInfo{static_cast<uint64_t>(length), {ret.torrent_name_}});
        ret.total_length_ = length;
    } else {
        // multi file mode
        ret.total_length_ = 0;
        if (info->try_object("files") == nullptr)
            return Err("torrent info missing files or length");
        auto* files = info->try_object("files")->try_list_object();
        if (files == nullptr) return Err("torrent info files is not a list");
        for (size_t i = 0; i < files->size(); i++) {
            auto* finfo = files->try_object(i)->try_map_object();
            if (finfo == nullptr) return Err("torrent info files element is not map");
            auto maybe_length = finfo->try_int("length");
            if (!maybe_length) return Err("torrent info files element missing length");

            if (finfo->try_object("path") == nullptr)
                return Err("torrent info files element missing path");
            auto* path = finfo->try_object("path")->try_list_object();
            if (path == nullptr) return Err("torrent info files element path is not a list");
            ASSIGN_OR_RAISE(auto path_v, ToStringVector(path));
            path_v.insert(path_v.begin(), ret.torrent_name_);
            ret.files_.push_back(FileInfo{static_cast<uint64_t>(*maybe_length), path_v});
            ret.total_length_ += *maybe_length;
        }
    }
    if (ret.piece_length_ * ret.GetPieceCount() - ret.total_length_ >= ret.piece_length_)
        return Err("torrent piece count not match " +
                   absl::StrCat("total:", ret.total_length_, " piece_len:", ret.piece_length_,
                                " hash_len:", ret.hash_pool_.length(),
                                " piece_count:", ret.GetPieceCount()));
    ret.data_ = std::move(parsed);
    return ret;
}

Result<TorrentFile> TorrentFile::LoadFile(absl::string_view file_path) {
    ifstream ifile{string{file_path}, ios::binary};
    if (ifile) {
        std::string data{std::istreambuf_iterator<char>(ifile), std::istreambuf_iterator<char>{}};
        return TorrentFile::Load(data);
    } else {
        return Err(absl::StrCat("failed to open torrent file: ", file_path));
    }
}

}  // namespace ryu
