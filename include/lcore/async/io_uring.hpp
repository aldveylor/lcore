#pragma once
#if !defined(__linux__)
#error "io_uring is only supported on Linux"
#else
#include "executor.hpp"
#include "mutex.hpp"
#include "lcore/enum.hpp"
#include "lcore/container/view.hpp"
#include "lcore/string.hpp"
#include <sys/socket.h>
#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <filesystem>
#include <liburing.h>

LCORE_NAMESPACE_BEGIN
namespace async {

class IOUringComponent: public Component {
public:
    struct CoroutineData {
        std::coroutine_handle<> handle;
        int* result;
    };
private:
    std::thread m_worker;
    eventfd_t m_eventfd;
    io_uring m_ring;

    bool m_running;

    uint64_t m_next_operation_id;

    std::unordered_map<uint64_t, CoroutineData> m_pending_operations;

    Scheduler* m_scheduler;
    std::size_t m_queue_depth;

    void WorkerThread();
protected:
    void DoInitialize(Scheduler& schedule) override;
    void DoFinalize(Scheduler&) override;

    struct io_uring_sqe* GetSQE();
    void CommitSQE(CoroutineData data, struct io_uring_sqe* sqe);
public:
    IOUringComponent(std::size_t queue_depth = 64);
    IOUringComponent(const IOUringComponent&) = delete;
    ~IOUringComponent();

    IOUringComponent& operator=(const IOUringComponent&) = delete;

    using Handle = std::coroutine_handle<>;

    void SubmitOperation(CoroutineData data, const struct io_uring_sqe& sqe);

    void SubmitRead(CoroutineData data, int fd, void* buffer, size_t size, uint64_t offset);
    void SubmitWrite(CoroutineData data, int fd, const void* buffer, size_t size, uint64_t offset);

    void SubmitAccept(CoroutineData data, int fd, sockaddr* addr, socklen_t* addrlen, int flags);
    void SubmitConnect(CoroutineData data, int fd, const sockaddr* addr, socklen_t addrlen);
    void SubmitSend(CoroutineData data, int fd, const void* buffer, size_t size, int flags);
    void SubmitRecv(CoroutineData data, int fd, void* buffer, size_t size, int flags);

    void SubmitOpenAt(CoroutineData data, int dirfd, const char* pathname, int flags, mode_t mode);
    void SubmitMkdirAt(CoroutineData data, int dirfd, const char* pathname, mode_t mode);
    void SubmitClose(CoroutineData data, int fd);
    void SubmitFsync(CoroutineData data, int fd);
    void SubmitStatx(CoroutineData data, int dirfd, const char* pathname, int flags, unsigned int mask, struct statx* statxbuf);

};

enum class OpenMode {
    ReadOnly = O_RDONLY,
    WriteOnly = O_WRONLY,
    ReadWrite = O_RDWR
};
enum class FileFlags {
    None = 0,
    Create = O_CREAT,
    Truncate = O_TRUNC,
    Append = O_APPEND,
    Exclusive = O_EXCL,
    Directory = O_DIRECTORY,
    Sync = O_SYNC,
    DSync = O_DSYNC,
    NoFollow = O_NOFOLLOW,
    NonBlock = O_NONBLOCK,
};
enum class FilePermission {
    Read = 4,
    Write = 2,
    Execute = 1
};
enum class SeekWhence {
    Set = SEEK_SET,
    Current = SEEK_CUR,
    End = SEEK_END
};

class AsyncFile {
    int m_fd = -1;
    bool m_ownership = true;
    Offset m_current_offset = 0;
    AsyncMutex m_mutex;
protected:
    AsyncFile(int fd): m_fd(fd) {}
public:
    using Mode = OpenMode;
    using Flags = FileFlags;
    using Permissions = mode_t;
    
    static constexpr Permissions DefaultPermissions = 0644;
    static constexpr Permissions DefaultDirectoryPermissions = 0755;

    AsyncFile(const std::filesystem::path& path, Mode mode = Mode::ReadOnly, Flags flags = Flags::None);
    AsyncFile(const std::filesystem::path& path, Mode mode, Flags flags, Permissions permissions);

    AsyncFile(const AsyncFile&) = delete;
    AsyncFile(AsyncFile&& other) noexcept;
    ~AsyncFile();

    AsyncFile& operator=(const AsyncFile&) = delete;
    AsyncFile& operator=(AsyncFile&& other) noexcept;

    static AsyncFile FromFD(int fd, bool take_ownership = false);

    int GetFD() const { return m_fd; }
    Lazy<std::size_t> Read(Span<char> buffer);
    Lazy<std::size_t> Write(Span<const char> buffer);
    Lazy<void> Seek(Offset offset, SeekWhence whence = SeekWhence::Set);
    Offset Tell() const { return m_current_offset; }
    Lazy<std::size_t> ReadAt(Offset offset, Span<char> buffer);
    Lazy<std::size_t> WriteAt(Offset offset, Span<const char> buffer);
    Lazy<void> Fsync();
    Lazy<void> Close() &&;

    Lazy<AsyncFile> OpenAt(StringView path, Mode mode = Mode::ReadOnly, Flags flags = Flags::None);
    Lazy<AsyncFile> OpenAt(StringView path, Mode mode, Flags flags, Permissions permissions);
    Lazy<void> MkdirAt(StringView path, Permissions mode = DefaultDirectoryPermissions);
};

}

LCORE_ENUM_BITWISE_OPERATORS(async::FileFlags)
LCORE_ENUM_BITWISE_OPERATORS(async::FilePermission)

LCORE_NAMESPACE_END


#endif

