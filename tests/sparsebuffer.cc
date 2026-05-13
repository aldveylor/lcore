#include <gtest/gtest.h>

#include "lcore/sparsebuffer.hpp"

using namespace lcore;

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(SparseBuffer, Basic)
{
    SparseBuffer<int> buf(10240);

    EXPECT_EQ(buf.size(), 10240);
    EXPECT_EQ(buf.chunk_count(), 0); // No chunks allocated yet

    int data[1024] = {0};
    for (int i = 0; i < 1024; ++i)
        data[i] = i;

    buf.write(1024, data);
    EXPECT_EQ(buf.chunk_count(), 1); // One chunk allocated
    EXPECT_EQ(buf.size(), 10240);
    
    int refbuf[2048];
    Span<int> ref(refbuf);
    auto readsize = buf.read(1024 + 512, ref);
    EXPECT_EQ(ref[0], 512);
    EXPECT_EQ(ref[511], 1023);
    EXPECT_EQ(buf.chunk_count(), 1); // Still one chunk allocated

    buf.write(3072, data);
    EXPECT_EQ(buf.chunk_count(), 2); // Second chunk allocated
    EXPECT_EQ(buf.size(), 10240);

    readsize = buf.read(2048, ref);
    EXPECT_EQ(buf.chunk_count(), 2); // Still two chunks allocated

    buf.write_sparse(1024 + 512, data);
    EXPECT_EQ(buf.chunk_count(), 2); // Still two chunks allocated
    EXPECT_EQ(buf.size(), 10240);
    readsize = buf.read(1024 + 512, ref);
    EXPECT_EQ(ref[0], 512);
    EXPECT_EQ(ref[1023], 1023);
    EXPECT_EQ(buf.chunk_count(), 2); // Still two chunks allocated
    
    // Write across chunk boundary
    buf.write(2048 + 512, data);
    EXPECT_EQ(buf.chunk_count(), 1); // Chunks merged
    EXPECT_EQ(buf.size(), 10240);
    readsize = buf.read(2048, ref);
    EXPECT_EQ(readsize, 2048); // All data is now available
    
    buf.clear();
    EXPECT_EQ(buf.chunk_count(), 0); // All chunks cleared
    EXPECT_EQ(buf.size(), 0);
}
