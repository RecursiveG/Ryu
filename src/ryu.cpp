#include <fstream>
#include <iostream>
#include <string>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "torrent_file.h"

using namespace std;
using namespace ryu;

ABSL_FLAG(string, torrent_file, "", "Torrent file path");
ABSL_FLAG(bool, dump_json, false, "Only dump json of the torrent file");
ABSL_FLAG(bool, show_piece_hash, false, "Display hash for all pieces");

void work(string path) {
    if (absl::GetFlag(FLAGS_dump_json)) {
        ifstream ifile{path, ios::binary};
        if (ifile) {
            std::string data{std::istreambuf_iterator<char>(ifile),
                             std::istreambuf_iterator<char>{}};
            size_t idx = 0;
            auto parsed = bencode::BencodeObject::parse(data, &idx).TakeOrRaise();
            auto json = parsed->json().TakeOrRaise();
            cout << json << endl;
        } else {
            std::cout << "failed to open file" << endl;
        }
    } else {
        auto MaybeTimeToStr = [](std::optional<absl::Time> t) -> string {
            if (!t) return "(-- no data --)";
            return absl::FormatTime(*t, absl::LocalTimeZone());
        };
        auto torrent = TorrentFile::LoadFile(path).TakeOrRaise();
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
    }
}

int main(int argc, char* argv[]) {
    absl::ParseCommandLine(argc, argv);
    string path = absl::GetFlag(FLAGS_torrent_file);
    if (path.empty()) {
        cout << "no torrent file specified" << endl;
    } else {
        work(path);
    }
    return 0;
}