#pragma once

#include <uv.h>

#include <array>
#include <string>

namespace ryu {

class App;
class Task {
  public:
    enum class State {
        OPENING,
        READING,
        READED,

        ERROR,
    };

    Task(App* app, uv_loop_t* loop, std::string torrent_file_name);
    void TorrentFileOpenCb(uv_fs_t* req);
    void TorrentFileReadCb(uv_fs_t* req);

  private:
    App* app_;
    uv_loop_t* loop_;
    State state_;

    std::string torrent_file_name_;
    uv_fs_t torrent_file_;
    uv_file torrent_file_fd_;
    std::string torrent_file_content_;
    std::array<char, 4096> torrent_read_buf_;
};

}  // namespace ryu