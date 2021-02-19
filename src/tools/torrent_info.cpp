#include <cpr/cpr.h>
#include <unistd.h>

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <utility>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/flags/usage.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "os.h"
#include "sha1.h"
#include "torrent_file.h"
#include "trackers.h"

namespace fs = std::filesystem;
using namespace std;
using namespace ryu;

ABSL_FLAG(string, torrent_file, "", "Torrent file path");
ABSL_FLAG(bool, dump_json, false, "Only dump json of the torrent file");
ABSL_FLAG(bool, show_piece_hash, false, "Display hash for all pieces");
ABSL_FLAG(string, verify, "", "File or folder to verify against the torrent");
ABSL_FLAG(int, query_peers, -1, "Query Nth tracker for peer list");

void work(string path) {
    if (absl::GetFlag(FLAGS_dump_json)) {
        ifstream ifile{path, ios::binary};
        if (ifile) {
            std::string data{std::istreambuf_iterator<char>(ifile),
                             std::istreambuf_iterator<char>{}};
            size_t idx = 0;
            auto parsed = bencode::BencodeObject::Parse(data, &idx).Expect("unable to parse data");
            auto json = parsed->Json().Expect("unable to serialize to json");
            cout << json << endl;
        } else {
            std::cout << "failed to open file" << endl;
        }
    } else {
        auto MaybeTimeToStr = [](std::optional<absl::Time> t) -> string {
            if (!t) return "(-- no data --)";
            return absl::FormatTime(*t, absl::LocalTimeZone());
        };
        auto torrent = TorrentFile::LoadFile(path).Expect("unable to load torrent file");
        cout << "Torrent name: " << torrent.name() << endl;
        cout << "Announce URLs:" << endl;
        cout << "  - " << torrent.announce() << endl;
        auto al = torrent.announce_list();
        if (al) {
            for (const auto& g : *al) {
                for (const auto& u : g) {
                    cout << "  - " << u << endl;
                }
            }
        }
        cout << "Created: " << MaybeTimeToStr(torrent.creation_date()) << endl;
        cout << "Created by: " << torrent.created_by().value_or("(-- no data --)") << endl;
        cout << "Comment: " << torrent.comment().value_or("(-- no data --)") << endl;
        cout << "InfoHash: " << torrent.GetInfoHexHash() << endl;
        cout << absl::StrFormat("There are %u(%.02fMB) files, %u pieces. Piece size %.02fKB",
                                torrent.GetFileCount(), torrent.GetTotalSize() / 1024.0 / 1024.0,
                                torrent.GetPieceCount(), torrent.GetPieceSize() / 1024.0)
             << endl;
        for (size_t i = 0; i < torrent.GetFileCount(); i++) {
            cout << absl::StrFormat("File #%03u %8.02fMB %s", i + 1,
                                    torrent.GetFileInfo(i).length / 1024.0 / 1024.0,
                                    absl::StrJoin(torrent.GetFileInfo(i).path, "/"))
                 << endl;
        }
        if (absl::GetFlag(FLAGS_show_piece_hash)) {
            for (size_t i = 0; i < torrent.GetPieceCount(); i++) {
                cout << absl::StrFormat("Piece #%04u %8.02fKB HASH=%s", i + 1,
                                        torrent.GetPieceSize(i) / 1024.0,
                                        torrent.GetPieceHexHash(i))
                     << endl;
            }
        }
        if (absl::GetFlag(FLAGS_query_peers) >= 0) {
            int idx = absl::GetFlag(FLAGS_query_peers);
            string announce = (idx == 0) ? torrent.announce() : torrent.announce_list()->at(idx-1)[0];
            cout << "Quering: " << announce << endl;
            auto tracker_reply = Trackers::GetPeers(announce, torrent.GetInfoHash(),
                                                    torrent.GetTotalSize())
                                     .Expect("unable to parse tracker reply");
            if (tracker_reply.failure_reason.empty()) {
                cout << "Tracker interval: " << tracker_reply.interval << endl;
                for (size_t i = 0; i < tracker_reply.peers.size(); i++) {
                    string pid = tracker_reply.peers[i].peer_id.empty()
                                     ? string("(----no-peer-id----)")
                                     : tracker_reply.peers[i].peer_id;
                    cout << absl::StrFormat("Peer #%03u %s port=% 5u %s", i + 1, pid,
                                            tracker_reply.peers[i].port, tracker_reply.peers[i].ip)
                         << endl;
                }
            } else {
                cout << "Tracker failure: " << tracker_reply.failure_reason << endl;
            }
        }
    }
}

void verify(const string& torrent_path, fs::path root_folder_path) {
    auto torrent = TorrentFile::LoadFile(torrent_path).Expect("unable to load torrent file");
    auto GetFilePath = [&root_folder_path, &torrent](size_t index) {
        auto new_path = root_folder_path;
        const auto& path_comp = torrent.GetFileInfo(index).path;
        for (size_t i = 1; i < path_comp.size(); i++) new_path /= path_comp[i];
        return new_path;
    };

    size_t current_file = 0;
    auto fd = os::AutoFd::open(GetFilePath(current_file), O_RDONLY).Expect("failed to open file");
    auto buf = std::make_unique<uint8_t[]>(torrent.GetPieceSize());

    auto fill_buffer = [&](uint64_t read_size) {
        uint64_t offset = 0;
        while (offset < read_size) {
            uint64_t remaining = read_size - offset;
            ssize_t red = read(fd.get(), buf.get() + offset, remaining);
            if (red < 0) {
                throw runtime_error(
                    absl::StrCat("fail to read from fd ", fd.get(), " ", strerror(errno)));
            } else if (red == 0) {
                fd = os::AutoFd::open(GetFilePath(++current_file), O_RDONLY)
                         .Expect("failed to open file");
            } else {
                offset += red;
            }
        }
    };

    SHA1 hasher{};
    auto start_time = absl::Now();
    uint64_t processed_bytes = 0;
    double speed = 0;
    for (size_t current_piece = 0; current_piece < torrent.GetPieceCount(); current_piece++) {
        uint64_t piece_size = torrent.GetPieceSize(current_piece);
        fill_buffer(piece_size);
        hasher.reset();
        auto actual = hasher(buf.get(), piece_size);
        auto expected = torrent.GetPieceHexHash(current_piece);

        processed_bytes += piece_size;
        auto duration = absl::Now() - start_time;
        if (duration > absl::Seconds(1)) {
            speed = processed_bytes / absl::ToDoubleSeconds(duration) / 1024 / 1024;  // MB/s
            start_time = absl::Now();
            processed_bytes = 0;
        }

        cout << absl::StrFormat("Piece #%04u %8.02fKB HASH=%s", current_piece + 1,
                                piece_size / 1024.0, expected);
        if (actual == expected)
            cout << absl::StrFormat(" (verified % 6.02fMB/s)", speed) << endl;
        else
            cout << absl::StrFormat(" (failed, actual=%s)", actual) << endl;
    }
}

int main(int argc, char* argv[]) {
    absl::SetProgramUsageMessage("--torrent_file [--verify | --dump_json | [--query_peers] [-show_piece_hash]]");
    absl::ParseCommandLine(argc, argv);
    string torrent_path = absl::GetFlag(FLAGS_torrent_file);
    string verify_path = absl::GetFlag(FLAGS_verify);

    if (torrent_path.empty()) {
        cout << "no torrent file specified" << endl;
        return 0;
    } else if (!verify_path.empty()) {
        verify(torrent_path, verify_path);
    } else {
        work(torrent_path);
    }
    return 0;
}