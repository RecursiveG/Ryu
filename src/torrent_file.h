#ifndef RYU_TORRENT_FILE_H
#define RYU_TORRENT_FILE_H

#include <cinttypes>
#include <optional>
#include <stdexcept>
#include <vector>
#include <string>

#include "absl/strings/str_format.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "common/bencode.h"
#include "result.h"

namespace ryu {

struct FileInfo {
    uint64_t length;
    std::vector<std::string> path;  // the last segment is always the name of the file
};

namespace {
std::string ToHex(absl::string_view buf) {
    std::string ret;
    for (uint8_t b : buf) {
        absl::StrAppendFormat(&ret, "%02x", b);
    }
    return ret;
}
}  // namespace

class TorrentFile {
  public:
    static constexpr size_t HASH_LENGTH = 20;  // a single un-encoded SHA-1 is 20 bytes long
    static Result<TorrentFile, std::string> Load(const std::string& bytes);
    static Result<TorrentFile, std::string> LoadFile(absl::string_view file_path);

    TorrentFile() = default;

    [[nodiscard]] std::string name() const { return torrent_name_; }
    [[nodiscard]] std::string announce() const { return announce_; }
    [[nodiscard]] std::optional<std::vector<std::vector<std::string>>> announce_list() const {
        return alt_announce_list_;
    }
    [[nodiscard]] std::optional<absl::Time> creation_date() const {
        auto maybe_date = (*data_)["creation date"].GetInt();
        if (maybe_date) {
            return absl::FromUnixSeconds(*maybe_date);
        } else {
            return {};
        }
    }
    [[nodiscard]] std::optional<std::string> comment() const {
        return (*data_)["comment"].GetString();
    }
    [[nodiscard]] std::optional<std::string> created_by() const {
        return (*data_)["created by"].GetString();
    }
    [[nodiscard]] size_t GetTotalSize() const { return total_length_; }

    [[nodiscard]] size_t GetFileCount() const { return files_.size(); }
    [[nodiscard]] FileInfo GetFileInfo(size_t index) const { return files_.at(index); }

    [[nodiscard]] size_t GetPieceCount() const { return hash_pool_.size() / HASH_LENGTH; }
    [[nodiscard]] std::string GetPieceHash(size_t index) const {
        if (index >= GetPieceCount())
            throw std::out_of_range(
                absl::StrFormat("hash index %u out of bound, max %u", index, GetPieceCount()));
        return hash_pool_.substr(index * HASH_LENGTH, HASH_LENGTH);
    }
    [[nodiscard]] std::string GetPieceHexHash(size_t index) const {
        return ToHex(GetPieceHash(index));
    }
    [[nodiscard]] size_t GetPieceSize() const { return piece_length_; }
    [[nodiscard]] size_t GetPieceSize(size_t index) const {
        if (index >= GetPieceCount())
            throw std::out_of_range(
                absl::StrFormat("hash index %u out of bound, max %u", index, GetPieceCount()));
        if (index == GetPieceCount() - 1)
            return total_length_ - (GetPieceCount() - 1) * piece_length_;
        return piece_length_;
    }
    std::string GetInfoHash() const { return info_hash_; }
    std::string GetInfoHexHash() const { return ToHex(GetInfoHash()); }

    void Dump(bool list_all_hashes = false);

  private:
    std::unique_ptr<bencode::BencodeObject> data_;

    std::string announce_;
    std::optional<std::vector<std::vector<std::string>>> alt_announce_list_;
    size_t piece_length_{};
    size_t total_length_{};
    std::string torrent_name_;
    std::vector<FileInfo> files_;
    std::string hash_pool_;
    std::string info_hash_;
};
}  // namespace ryu

#endif  // RYU_TORRENT_FILE_H
