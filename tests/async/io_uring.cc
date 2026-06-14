/// require(linux)
#include <exception>
#include <gtest/gtest.h>
#include "lcore/async/io_uring.hpp"
#include "timeout.hpp"
#include <chrono>
#include <sstream>

int main(int argc, char** argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::async;

using namespace std::chrono_literals;

void EnsureIOUringComponent() {
    static bool _ = []() {
        auto& scheduler = Scheduler::GetInstance();
        scheduler.AttachComponent<IOUringComponent>();
        return true;
    }();
}

const std::filesystem::path tempDir = std::filesystem::temp_directory_path() / "lcore_test_async_io_uring";

TEST(AsyncIO, AsyncFile) {
    EnsureIOUringComponent();
    auto& scheduler = Scheduler::GetInstance();
    auto _ = crash_after(5s);
    std::exception_ptr ex;
    scheduler.exceptionHandler=[&](std::exception_ptr e) {
        ex = e;
    };

    if (std::filesystem::exists(tempDir)) {
        if (std::filesystem::remove_all(tempDir) == 0) {
            FAIL() << "Failed to remove existing temp directory: " << tempDir;
        }
    }

    auto main = []() -> Lazy<> {
        if (std::filesystem::create_directories(tempDir) == false) {
            throw RuntimeError("Failed to create temp directory");
        }

        auto root = AsyncFile(tempDir);
        auto file1 = co_await root.OpenAt("file1", OpenMode::ReadWrite, FileFlags::Create);

        const char* data = "Hello, world!";
        auto str = StringView(data);

        auto writedLen = co_await file1.Write(str);
        auto curOffset = file1.Tell();
        EXPECT_EQ(writedLen, 13);
        EXPECT_EQ(curOffset, 13);
        co_await file1.Fsync();

        char buf[20] = {};
        Span<char> readBuf(buf);
        auto readLen = co_await file1.Read(readBuf);
        EXPECT_EQ(readLen, 0);
        co_await file1.Seek(0);
        readLen = co_await file1.Read(readBuf);
        EXPECT_EQ(readLen, 13);
        EXPECT_EQ(StringView(buf, readLen), "Hello, world!");

        co_await root.MkdirAt("fod1", 0755);
        auto fod1 = co_await root.OpenAt("fod1", OpenMode::ReadOnly, FileFlags::Directory);
        auto file2 = co_await fod1.OpenAt("file2", OpenMode::ReadWrite, FileFlags::Create);
        writedLen = co_await file2.Write(str);
        EXPECT_EQ(writedLen, 13);


    }();

    scheduler.Schedule(std::move(main));
    scheduler.Run();

    if (ex) {
        try {
            std::rethrow_exception(ex);
        } catch (const Exception& e) {
            std::stringstream ss;
            for (auto& trace: e.GetBackTrace()) {
                ss << trace << "\n";
            }
            FAIL() << "Unexpected LCore exception: " << e.what() << "\nBacktrace:\n" << ss.str();
        } catch (const std::exception& e) {
            FAIL() << "Unexpected exception: " << e.what();
        } catch (...) {
            FAIL() << "Unexpected unknown exception";
        }
    }

    std::filesystem::remove_all(tempDir);
}
