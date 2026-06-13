#include <gtest/gtest.h>
#include <lcore/iostream.hpp>
#include <lcore/sstream.hpp>
#include <lcore/fstream.hpp>
#include <sstream>

using namespace LCORE_NAMESPACE;

/// Test for StringStream
TEST(StringStreamTest, InputStream) {
    IStringStream stream("Hello, World!");
    EXPECT_EQ(stream.View(), "Hello, World!");
    
    std::string str;
    stream >> str;
    EXPECT_EQ(str, "Hello,");

    str.clear();
    stream >> str;
    EXPECT_EQ(str, "World!");
}

TEST(StringStreamTest, OutputStream) {
    OStringStream stream;
    stream << "Hello, World!";
    EXPECT_EQ(stream.View(), "Hello, World!");

    std::string str = stream.Str();
    EXPECT_EQ(str, "Hello, World!");

    stream.SeekOff(-6, IOSBase::SeekDir::End);
    stream << "C++20";
    EXPECT_EQ(stream.View(), "Hello, C++20!");
}

TEST(StringStreamTest, IOStream) {
    IOStringStream stream("Initial String");
    EXPECT_EQ(stream.View(), "Initial String");

    stream << " Appended Text";
    EXPECT_EQ(stream.View(), "Initial String Appended Text");

    std::string str = stream.Str();
    EXPECT_EQ(str, "Initial String Appended Text");

    std::string input;
    stream >> input;
    EXPECT_EQ(input, "Initial"); 
}

class FStreamTest : public ::testing::Test {
protected:
    std::filesystem::path testfolder;

    void SetUp() override {
        testfolder = std::filesystem::temp_directory_path() / "lcore_fstream_test";
        std::filesystem::create_directories(testfolder);

        // Create a test read file
        std::ofstream outFile(testfolder / "test_read.txt");
        outFile << "Hello, World!" << std::endl;
        outFile << "This is a test file." << std::endl;
        outFile.close();
    }
    void TearDown() override {
        std::filesystem::remove_all(testfolder);
    }
};

TEST_F(FStreamTest, IFStream) {
    IFStream inFile(testfolder / "test_read.txt");
    std::string line;
    inFile >> line;
    EXPECT_EQ(line, "Hello,");
    inFile >> line;
    EXPECT_EQ(line, "World!");

    line = inFile.GetLine();
    EXPECT_EQ(line, "This is a test file.");

    inFile.SeekPos(7);
    inFile >> line;
    EXPECT_EQ(line, "World!");
}

TEST_F(FStreamTest, OFStream) {
    OFStream outFile(testfolder / "test_write.txt");
    outFile << "Writing to a file." << EndLine;
    outFile << "This is another line." << EndLine;
    outFile.SeekPos(0);

    // Verify the content
    std::ifstream inFile(testfolder / "test_write.txt");
    std::string line;
    std::getline(inFile, line);
    EXPECT_EQ(line, "Writing to a file.");
    std::getline(inFile, line);
    EXPECT_EQ(line, "This is another line.");

    outFile.SeekOff(0, IOSBase::SeekDir::End);
    outFile << "Appending to the file." << EndLine;

    // Verify the appended content
    inFile.clear(); // Clear EOF flag
    inFile.seekg(0, std::ios::beg);
    std::getline(inFile, line);
    EXPECT_EQ(line, "Writing to a file.");
    std::getline(inFile, line);
    EXPECT_EQ(line, "This is another line.");
    std::getline(inFile, line);
    EXPECT_EQ(line, "Appending to the file.");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
