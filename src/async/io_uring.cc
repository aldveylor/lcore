#ifdef __linux__
#include "lcore/async/io_uring.hpp"

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::async;

/// IOUringComponent implementation

void IOUringComponent::WorkerThread() {
    m_running = true;
    io_uring_queue_init(this->m_queue_depth, &this->m_ring, 0);
    this->m_eventfd = eventfd(0, EFD_NONBLOCK);
    io_uring_register_eventfd(&this->m_ring, this->m_eventfd);

    int epfd = epoll_create1(0);
    struct epoll_event event;
    event.events = EPOLLIN;
    event.data.fd = this->m_eventfd;
    epoll_ctl(epfd, EPOLL_CTL_ADD, this->m_eventfd, &event);

    while (m_running) {
        // Check for completion events
        struct io_uring_cqe* cqe;
        if (io_uring_peek_cqe(&this->m_ring, &cqe) == 0) {
            // Process the completion event
            auto userdata = cqe->user_data;
            if (auto it = this->m_pending_operations.find(userdata); it != this->m_pending_operations.end()) {
                auto cordata = it->second;
                *cordata.result = cqe->res; // Store the result
                // Notify back for completion
                this->m_scheduler->Schedule([handle = cordata.handle]() {
                    handle.resume(); // Resume the coroutine waiting for this operation
                });
                this->m_pending_operations.erase(it); // Remove from pending operations
            } else {
                // Unknown user data
                std::cerr << "Unknown user data in completion event: " << userdata << std::endl;
                std::terminate();
            }
            io_uring_cqe_seen(&this->m_ring, cqe);
        }

        // Wait for events
        while (true) {
            struct epoll_event events[1];
            epoll_wait(epfd, events, 1, -1);
            // Process events
            if ((unsigned long)events[0].data.fd == this->m_eventfd) {
                break; // Exit the loop to process events
            }
        }
    }
    // Cleanup
    io_uring_queue_exit(&this->m_ring);
    close(this->m_eventfd);
    m_running = false;
}

void IOUringComponent::DoInitialize(Scheduler& scheduler) {
    if (this->m_worker.joinable()) {
        throw RuntimeError("IOUringComponent is already initialized");
    }
    this->m_scheduler = &scheduler;
    this->m_worker = std::thread(&IOUringComponent::WorkerThread, this);
}

void IOUringComponent::DoFinalize(Scheduler&) {
    if (!this->m_worker.joinable()) {
        throw RuntimeError("IOUringComponent is not initialized");
    }
    m_running = false;
    // Wake up the worker thread to exit
    uint64_t one = 1;
    write(this->m_eventfd, &one, sizeof(one));
    this->m_worker.join();
    this->m_scheduler = nullptr;
}

IOUringComponent::IOUringComponent(std::size_t queue_depth) {
    if (queue_depth == 0) {
        throw InvalidArgument("Queue depth must be greater than 0");
    }
    this->m_queue_depth = queue_depth;
}
IOUringComponent::~IOUringComponent() {
    if (this->m_worker.joinable()) {
        std::cerr << "Warning: IOUringComponent is being destroyed while still running. Finalizing..." << std::endl;
        this->DoFinalize(*this->m_scheduler); // Attempt to finalize if still running
    }
}

struct io_uring_sqe* IOUringComponent::GetSQE() {
    struct io_uring_sqe* sqe = io_uring_get_sqe(&this->m_ring);
    if (!sqe) {
        throw RuntimeError("Failed to get SQE");
    }
    return sqe;
}

void IOUringComponent::CommitSQE(CoroutineData data, struct io_uring_sqe* sqe) {
    auto userdata = m_next_operation_id++;
    m_pending_operations.insert({userdata, data}); // Store the coroutine data for this operation

    sqe->user_data = userdata; // Set the user data
    io_uring_submit(&this->m_ring); // Submit the operation
}

void IOUringComponent::SubmitOperation(CoroutineData data, const struct io_uring_sqe& sqe) {
    struct io_uring_sqe* sqe_ptr = this->GetSQE();
    *sqe_ptr = sqe; // Copy the provided SQE to the one obtained from the ring
    this->CommitSQE(data, sqe_ptr); // Commit the SQE with the provided coroutine data
}

void IOUringComponent::SubmitRead(CoroutineData data, int fd, void* buffer, size_t size, uint64_t offset) {
    auto sqe = GetSQE();
    io_uring_prep_read(sqe, fd, buffer, size, offset);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitWrite(CoroutineData data, int fd, const void* buffer, size_t size, uint64_t offset){
    auto sqe = GetSQE();
    io_uring_prep_write(sqe, fd, buffer, size, offset);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitAccept(CoroutineData data, int fd, sockaddr* addr, socklen_t* addrlen, int flags){
    auto sqe = GetSQE();
    io_uring_prep_accept(sqe, fd, addr, addrlen, flags);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitConnect(CoroutineData data, int fd, const sockaddr* addr, socklen_t addrlen){
    auto sqe = GetSQE();
    io_uring_prep_connect(sqe, fd, addr, addrlen);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitSend(CoroutineData data, int fd, const void* buffer, size_t size, int flags) {
    auto sqe = GetSQE();
    io_uring_prep_send(sqe, fd, buffer, size, flags);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitRecv(CoroutineData data, int fd, void* buffer, size_t size, int flags) {
    auto sqe = GetSQE();
    io_uring_prep_recv(sqe, fd, buffer, size, flags);
    this->CommitSQE(data, sqe);
}

void IOUringComponent::SubmitOpenAt(CoroutineData data, int dirfd, const char* pathname, int flags, mode_t mode){
    auto sqe = GetSQE();
    io_uring_prep_openat(sqe, dirfd, pathname, flags, mode);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitMkdirAt(CoroutineData data, int dirfd, const char* pathname, mode_t mode) {
    auto sqe = GetSQE();
    io_uring_prep_mkdirat(sqe, dirfd, pathname, mode);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitClose(CoroutineData data, int fd) {
    auto sqe = GetSQE();
    io_uring_prep_close(sqe, fd);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitFsync(CoroutineData data, int fd) {
    auto sqe = GetSQE();
    io_uring_prep_fsync(sqe, fd, 0);
    this->CommitSQE(data, sqe);
}
void IOUringComponent::SubmitStatx(CoroutineData data, int dirfd, const char* pathname, int flags, unsigned int mask, struct statx* statxbuf) {
    auto sqe = GetSQE();
    io_uring_prep_statx(sqe, dirfd, pathname, flags, mask, statxbuf);
    this->CommitSQE(data, sqe);
}
/// AsyncFile implementation
AsyncFile::AsyncFile(const std::filesystem::path& path, Mode openmode, Flags flags) {
    Permissions mode = 0;
    if ((bool)(flags & Flags::Create)) {
        if ((bool)(flags & Flags::Directory)) {
            mode = DefaultDirectoryPermissions; // Default permissions for directories
        } else {
            mode = DefaultPermissions; // Default permissions for files
        }
    }
    auto rawflag = static_cast<int>(flags) | (static_cast<int>(openmode) & O_ACCMODE);
    this->m_fd = open(path.c_str(), rawflag, mode);
    if (this->m_fd < 0) {
        throw SystemError();
    }
}
AsyncFile::AsyncFile(const std::filesystem::path& path, Mode mode, Flags flags, Permissions permissions){
    auto rawflag = static_cast<int>(flags) | (static_cast<int>(mode) & O_ACCMODE);
    this->m_fd = open(path.c_str(), rawflag, permissions);
    if (this->m_fd < 0) {
        throw SystemError();
    }
}
AsyncFile::~AsyncFile() {
    if (this->m_fd >= 0) {
        if (close(this->m_fd) < 0) {
            std::cerr << "Warning: Failed to close file descriptor " << this->m_fd << ": " << strerror(errno) << std::endl;
        }
    }
}

AsyncFile::AsyncFile(AsyncFile&& other) noexcept: m_fd(other.m_fd), m_current_offset(other.m_current_offset) {
    other.m_fd = -1; // Invalidate the moved-from object
}
AsyncFile& AsyncFile::operator=(AsyncFile&& other) noexcept {
    if (this != &other) {
        if (this->m_fd >= 0) {
            close(this->m_fd); // Close the existing file descriptor
        }
        this->m_fd = other.m_fd;
        this->m_current_offset = other.m_current_offset;
        other.m_fd = -1; // Invalidate the moved-from object
    }
    return *this;
}

struct CQEAwaiter {
    IOUringComponent& io_uring;
    int result = 0;
    
    inline CQEAwaiter(): io_uring(Scheduler::GetInstance().GetComponent<IOUringComponent>()) {}
    inline bool await_ready() const noexcept {
        return false; // Always suspend
    }
    inline auto GetCoroutineData(std::coroutine_handle<> handle) {
        return IOUringComponent::CoroutineData{handle, &result};
    }
    inline int await_resume() const noexcept {
        return result; // Return the result of the operation
    }
};

Lazy<std::size_t> AsyncFile::ReadAt(Offset offset, Span<char> buffer) {
    struct ReadAwaiter : public CQEAwaiter {
        int m_fd;
        Span<char> buffer;
        Offset offset;
        
        ReadAwaiter(int fd, Span<char> buffer, Offset offset): m_fd(fd), buffer(buffer), offset(offset) {}
        void await_suspend(std::coroutine_handle<> handle) {
            auto data = this->GetCoroutineData(handle);
            io_uring.SubmitRead(data, m_fd, buffer.data(), buffer.size(), offset);
        }
    };
    int res = co_await ReadAwaiter(this->m_fd, buffer, offset);
    if (res < 0) {
        throw SystemError(-res);
    }
    co_return res;
}
Lazy<std::size_t> AsyncFile::WriteAt(Offset offset, Span<const char> buffer) {
    struct WriteAwaiter : public CQEAwaiter {
        int m_fd;
        Span<const char> buffer;
        Offset offset;
        
        WriteAwaiter(int fd, Span<const char> buffer, Offset offset): m_fd(fd), buffer(buffer), offset(offset) {}
        void await_suspend(std::coroutine_handle<> handle) {
            auto data = this->GetCoroutineData(handle);
            io_uring.SubmitWrite(data, m_fd, buffer.data(), buffer.size(), offset);
        }
    };
    std::cout << "Submitting write operation: fd=" << this->m_fd << ", offset=" << offset << ", size=" << buffer.size() << std::endl;
    int res = co_await WriteAwaiter(this->m_fd, buffer, offset);
    if (res < 0) {
        throw SystemError(-res);
    }
    co_return res;
}
Lazy<void> AsyncFile::Fsync() {
    struct FsyncAwaiter : public CQEAwaiter {
        int m_fd;
        
        FsyncAwaiter(int fd): m_fd(fd) {}
        void await_suspend(std::coroutine_handle<> handle) {
            auto data = this->GetCoroutineData(handle);
            io_uring.SubmitFsync(data, m_fd);
        }
    };
    co_await FsyncAwaiter(this->m_fd);
}
Lazy<void> AsyncFile::Close() && {
    struct CloseAwaiter : public CQEAwaiter {
        int m_fd;
        
        CloseAwaiter(int fd): m_fd(fd) {}
        void await_suspend(std::coroutine_handle<> handle) {
            auto data = this->GetCoroutineData(handle);
            io_uring.SubmitClose(data, m_fd);
        }
    };
    co_await CloseAwaiter(this->m_fd);
}

Lazy<std::size_t> AsyncFile::Read(Span<char> buffer) {
    auto bytesRead = co_await this->ReadAt(m_current_offset, buffer);
    this->m_current_offset += bytesRead;
    co_return bytesRead;
}
Lazy<std::size_t> AsyncFile::Write(Span<const char> buffer) {
    auto bytesWritten = co_await this->WriteAt(m_current_offset, buffer);
    this->m_current_offset += bytesWritten;
    co_return bytesWritten;
}
Lazy<void> AsyncFile::Seek(Offset offset, SeekWhence whence) {
    if (offset == 0) {
        if (whence == SeekWhence::Current) {
            co_return; // No need to seek if offset is 0 and whence is current
        } else if (whence == SeekWhence::Set) {
            this->m_current_offset = 0; // Reset to the beginning of the file
            co_return;
        }
    }
    struct statx buf;
    struct StatxAwaiter : public CQEAwaiter {
        int m_fd;
        struct statx* buf;
        
        StatxAwaiter(int fd, struct statx* buf): m_fd(fd), buf(buf) {}
        void await_suspend(std::coroutine_handle<> handle) {
            auto data = this->GetCoroutineData(handle);
            io_uring.SubmitStatx(data, m_fd, "", AT_EMPTY_PATH, STATX_SIZE, buf); // Dummy statx to get file size
        }
    };
    int res = co_await StatxAwaiter(this->m_fd, &buf);
    if (res < 0) {
        throw SystemError(-res);
    }
    off_t fileSize = buf.stx_size;
    off_t newOffset;
    switch (whence) {
        case SeekWhence::Set:
            newOffset = offset;
            break;
        case SeekWhence::Current:
            newOffset = this->m_current_offset + offset;
            break;
        case SeekWhence::End:
            newOffset = fileSize + offset;
            break;
        default:
            throw InvalidArgument("Invalid seek whence");
    }
    if (newOffset < 0) {
        newOffset = 0; // Prevent seeking before the beginning of the file
    }
    if (newOffset > fileSize) {
        newOffset = fileSize; // Prevent seeking beyond the end of the file
    }
    this->m_current_offset = newOffset;
}

struct OpenAtAwaiter : public CQEAwaiter {
    int m_fd;
    StringView path;
    int flags;
    mode_t mode;
    
    OpenAtAwaiter(int fd, StringView path, int flags, mode_t mode): m_fd(fd), path(path), flags(flags), mode(mode) {}
    void await_suspend(std::coroutine_handle<> handle) {
        auto data = this->GetCoroutineData(handle);
        if (*path.end() == '0') // null-terminated
        io_uring.SubmitOpenAt(data, m_fd, path.data(), flags, mode);
        else {
            std::string tempPath(path.data(), path.size());
            io_uring.SubmitOpenAt(data, m_fd, tempPath.c_str(), flags, mode);
        }
    }
};
Lazy<AsyncFile> AsyncFile::OpenAt(StringView path, Mode openmode, Flags flags) {
    auto rawflag = static_cast<int>(flags) | (static_cast<int>(openmode) & O_ACCMODE);
    mode_t mode = 0;
    if ((bool)(flags & Flags::Create)) {
        if ((bool)(flags & Flags::Directory)) {
            mode = DefaultDirectoryPermissions; // Default permissions for directories
        } else {
            mode = DefaultPermissions; // Default permissions for files
        }
    }
    auto fd = co_await OpenAtAwaiter(m_fd, path, rawflag, mode);
    if (fd < 0) {
        throw SystemError(-fd);
    }
    co_return AsyncFile(fd);
}
Lazy<AsyncFile> AsyncFile::OpenAt(StringView path, Mode mode, Flags flags, Permissions permissions) {
    auto rawflag = static_cast<int>(flags) | (static_cast<int>(mode) & O_ACCMODE);
    auto fd = co_await OpenAtAwaiter(m_fd, path, rawflag, permissions);
    if (fd < 0) {
        throw SystemError(-fd);
    }
    co_return AsyncFile(fd);
}
Lazy<void> AsyncFile::MkdirAt(StringView path, Permissions mode) {
    struct MkdirAtAwaiter : public CQEAwaiter {
        int m_fd;
        StringView path;
        Permissions mode;
        
        MkdirAtAwaiter(int fd, StringView path, Permissions mode): m_fd(fd), path(path), mode(mode) {}
        void await_suspend(std::coroutine_handle<> handle) {
            auto data = this->GetCoroutineData(handle);
            if (*path.end() == '0') // null-terminated
                io_uring.SubmitMkdirAt(data, m_fd, path.data(), static_cast<mode_t>(mode));
            else {
                std::string tempPath(path.data(), path.size());
                io_uring.SubmitMkdirAt(data, m_fd, tempPath.c_str(), static_cast<mode_t>(mode));
            }
        }
    };
    int res = co_await MkdirAtAwaiter(m_fd, path, mode);
    if (res < 0) {
        throw SystemError(-res);
    }
}

#endif

