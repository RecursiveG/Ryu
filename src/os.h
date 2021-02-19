#ifndef RYU_OS_H
#define RYU_OS_H

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <cerrno>
#include <cstring>
#include <iostream>
#include <memory>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "result.h"

namespace ryu {
namespace os {
class AutoFd {
  public:
    static Result<AutoFd, std::string> open(const std::string& file_path, int mode) {
        int fd = ::open(file_path.c_str(), mode);
        if (fd < 0) RAISE_ERRNO("failed to open file: " + file_path);
        return AutoFd{fd};
    }

    AutoFd() : fd_(-1) {}
    explicit AutoFd(int fd) : fd_(fd) {}
    ~AutoFd() { close(fd_); }
    AutoFd(const AutoFd&) = delete;
    AutoFd& operator=(const AutoFd&) = delete;

    AutoFd(AutoFd&& another) noexcept {
        fd_ = another.fd_;
        another.fd_ = -1;
    };
    AutoFd& operator=(AutoFd&& another) noexcept {
        if (fd_ == another.fd_) {
            another.fd_ = -1;
        } else {
            close(fd_);
            fd_ = another.fd_;
            another.fd_ = -1;
        }
        return *this;
    };

    [[nodiscard]] int get() const { return fd_; }

  private:
    static void close(int fd) {
        if (fd < 0) return;
        int ret = ::close(fd);
        if (ret != 0) {
            std::cerr << absl::StrCat("failed to close fd:", fd, " ", strerror(errno));
        }
    }
    int fd_;
};
}  // namespace os
}  // namespace ryu

#endif  // RYU_OS_H
