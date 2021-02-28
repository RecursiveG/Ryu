#include "task.h"

#include <cassert>
#include <iostream>

#include "app.h"
#include "torrent_file.h"
#include "utils/uv_callbacks.h"
namespace ryu {

Task::Task(App* app, uv_loop_t* loop, std::string torrent_file_name)
    : app_(app), loop_(loop), state_(State::OPENING), torrent_file_name_(torrent_file_name) {
    int retcode;
    torrent_file_.data = this;
    retcode = uv_fs_open(loop_, &torrent_file_, torrent_file_name_.c_str(), UV_FS_O_RDONLY, 0755,
                         uv_callbacks::Fs<&Task::TorrentFileOpenCb>);
    assert(retcode == 0);
}

void Task::TorrentFileOpenCb(uv_fs_t* req) {
    assert(state_ == State::OPENING);
    assert(req == &torrent_file_);
    if (req->result < 0) {
        std::cout << "Failed to open: " << torrent_file_name_ << std::endl;
        state_ = State::ERROR;
        return;
    }
    torrent_file_fd_ = req->result;
    state_ = State::READING;

    uv_buf_t iovec{
        .base = torrent_read_buf_.data(),
        .len = torrent_read_buf_.size(),
    };
    int64_t offset = torrent_file_content_.size();
    int retcode = uv_fs_read(loop_, &torrent_file_, torrent_file_fd_, &iovec, 1, offset,
                             uv_callbacks::Fs<&Task::TorrentFileReadCb>);
    assert(retcode == 0);
}

void Task::TorrentFileReadCb(uv_fs_t* req) {
    int retcode;

    assert(state_ == State::READING);
    assert(req == &torrent_file_);
    if (req->result < 0) {
        std::cout << "Failed to read: " << torrent_file_name_ << std::endl;
        state_ = State::ERROR;
    } else if (req->result > 0) {
        // append and continue reading
        torrent_file_content_.append(torrent_read_buf_.data(), req->result);
        uv_buf_t iovec{
            .base = torrent_read_buf_.data(),
            .len = torrent_read_buf_.size(),
        };
        int64_t offset = torrent_file_content_.size();
        retcode = uv_fs_read(loop_, &torrent_file_, torrent_file_fd_, &iovec, 1, offset,
                             uv_callbacks::Fs<&Task::TorrentFileReadCb>);
        assert(retcode == 0);
    } else {
        // read complete
        retcode = uv_fs_close(loop_, &torrent_file_, torrent_file_fd_, nullptr);
        assert(retcode == 0);
        state_ = State::READED;
        std::cout << "Read " << torrent_file_content_.size()
                  << " bytes from file: " << torrent_file_name_ << std::endl;
        auto t = TorrentFile::Load(torrent_file_content_);
        if (!t) {
            std::cout << "Failed to parse torrent file" << std::endl;
        } else {
            t.Value().Dump();
        }
    }
}

}  // namespace ryu