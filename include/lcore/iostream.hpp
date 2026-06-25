#pragma once
#include "base.hpp"
#include "class.hpp"
#include "enum.hpp"
#include "exception.hpp"
#include "traits.hpp"
#include "pointer.hpp"
#include <iostream>

LCORE_NAMESPACE_BEGIN

class IOSBase: public AbstractClass {
public:
    enum class IOState {
        Good = 0,
        EndOfFile = 1,
        Fail = 2,
        Bad = 4
    };
    enum class SeekDir {
        Begin = std::ios_base::beg,
        Current = std::ios_base::cur,
        End = std::ios_base::end
    };
    enum class IOMode {
        In = std::ios_base::in,
        Out = std::ios_base::out
    };
    enum class OpenMode {
        Read = std::ios_base::in,
        Write = std::ios_base::out,
        Append = std::ios_base::app,
        Truncate = std::ios_base::trunc,
        Binary = std::ios_base::binary
    };
};

LCORE_ENUM_BITWISE_OPERATORS(IOSBase::IOState);
LCORE_ENUM_BITWISE_OPERATORS(IOSBase::OpenMode);

template <typename T>
struct BufferPointer {
    T* begin;
    T* current;
    T* end;

    void swap(BufferPointer& other) {
        std::swap(begin, other.begin);
        std::swap(current, other.current);
        std::swap(end, other.end);
    }
};

template <typename CharT>
class BasicStreamBufferBase {
public:
    using CharType = CharT;
    using BufferType = BufferPointer<CharType>;

    /// @brief Swap the current buffer with another
    void swap(BasicStreamBufferBase& other) {
        buffer.swap(other.buffer);
    }
protected:
    BufferType buffer; ///< The buffer for the stream

    BasicStreamBufferBase() = default;
public:
    BasicStreamBufferBase(BufferType buf)
        : buffer(std::move(buf)) {}

    virtual ~BasicStreamBufferBase() = default;
};

template <
    typename CharT,
    typename Traits = std::char_traits<CharT>>
class BasicIStreamBuffer: public BasicStreamBufferBase<CharT> {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
protected:
    BasicIStreamBuffer() = default;
public:
    BasicIStreamBuffer(BufferPointer<CharType> buf)
        : BasicStreamBufferBase<CharT>(std::move(buf)) {}
    ~BasicIStreamBuffer() { this->Sync(); }
public:
    // Virtual functions for stream buffer management
    // Seek
    /// @brief Seek to a specific position in the stream
    virtual PosType SeekPos(PosType pos) { LCORE_NOTIMPLEMENTED(); }
    /// @brief Seek to a specific offset from the current position in the stream
    virtual PosType SeekOff(OffType off, IOSBase::SeekDir dir) { LCORE_NOTIMPLEMENTED(); }
    // Get
    /// @brief Put back the last character read from the stream, if possible 
    virtual IntType Pbackfail(IntType c = Traits::eof()) {
        return Traits::eof(); // Defaultly return a failure
        // if (this->buffer.current == this->buffer.begin) return Traits::eof(); // No character to put back
        // --this->buffer.current; // Move back in the buffer
        // if (c != Traits::eof()) {
        //     *this->buffer.current = Traits::to_char_type(c); // Put back the character
        // }
        // return Traits::to_int_type(*this->buffer.current);
    }
    /// @brief Obtain the number of characters that can be read, if known
    virtual IntType Showmanyc() const { return Traits::eof(); }
protected:
    /// @brief The buffer is underflowed, this function is called to read more data into the buffer
    virtual IntType Underflow() {
        // assert(buffer.current == buffer.end, "Buffer not underflowed when calling Underflow");
        return Traits::eof(); // No more data to read
    }
    /// @brief The buffer is underflowed, this function is called to read a character from the stream
    virtual IntType Uflow() {
        // assert(buffer.current == buffer.end, "Buffer not underflowed when calling Uflow");
        if (Underflow() == Traits::eof()) return Traits::eof(); // No more data to read
        return Traits::to_int_type(*this->buffer.current++);
    }
public:
    /// @brief Synchronize the stream buffer with the device
    virtual int Sync() { return 0; }
    /// @brief Get multiple characters from the stream
    /// @return std::streamsize The number of characters read, if this value is less than n, it means the end of the stream is reached.
    /// If a error is encountered, it will throw an exception.
    virtual std::streamsize SGetN(CharType* s, std::streamsize n) {
        std::streamsize count = 0;
        while (count < n) {
            if (this->buffer.current == this->buffer.end) {
                if (Underflow() == Traits::eof()) break; // No more data to read
            }
            *s++ = *this->buffer.current++;
            ++count;
        }
        return count; // Return the number of characters read
    }


    // Implementation details
    // Peek a character from the stream without removing it
    IntType SBumpC() {
        if (this->buffer.current == this->buffer.end) this->Underflow();
        return Traits::to_int_type(*this->buffer.current);
    }
    // Get a character from the stream and remove it
    IntType SGetC() {
        if (this->buffer.current == this->buffer.end) return this->Uflow();
        return Traits::to_int_type(*this->buffer.current++);
    }
};

template <
    typename CharT,
    typename Traits = std::char_traits<CharT>>
class BasicOStreamBuffer: public BasicStreamBufferBase<CharT> {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
protected:
    BasicOStreamBuffer() = default;
public:
    BasicOStreamBuffer(BufferPointer<CharType> buf)
        : BasicStreamBufferBase<CharT>(std::move(buf)) {}
    ~BasicOStreamBuffer() { this->Sync(); }

    // Virtual functions for stream buffer management
    // Seek
    /// @brief Seek to a specific position in the stream
    virtual PosType SeekPos(PosType pos) { LCORE_NOTIMPLEMENTED(); }
    /// @brief Seek to a specific offset from the current position in the stream
    virtual PosType SeekOff(OffType off, IOSBase::SeekDir dir) { LCORE_NOTIMPLEMENTED(); }
    // Put
    /// @brief Put multiple characters into the stream
    virtual std::streamsize SPutN(const CharType* s, std::streamsize n) { return 0; }
protected:
    /// @brief The buffer is overflowed, this function is called to write a character to the stream
    virtual IntType Overflow(IntType c = Traits::eof()) {
        // assert(buffer.current == buffer.end, "Buffer not overflowed when calling Overflow");
        return Traits::eof(); // Defaultly return a failure
    }
public:
    /// @brief Synchronize the stream buffer with the device
    virtual int Sync() { return 0; }

    // Implementation details
    // Put a character into the stream
    IntType SPutC(CharType c) {
        if (this->buffer.current == this->buffer.end) {
            if (Overflow(Traits::to_int_type(c)) == Traits::eof())
                return Traits::eof(); // No more space in the buffer, overflow
        } else {
            *this->buffer.current++ = c; // Put the character into the buffer
        }
        return Traits::to_int_type(c);
    }
};

// Standard IO

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicIOS: public IOSBase {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;

    using IStreamBuffer = BasicIStreamBuffer<CharType, Traits>;
    using OStreamBuffer = BasicOStreamBuffer<CharType, Traits>;
private:
    IStreamBuffer* m_inputBuffer = nullptr; ///< Input stream buffer
    OStreamBuffer* m_outputBuffer = nullptr; ///< Output stream buffer
public:
    BasicIOS() = default;
    virtual ~BasicIOS() = default;

    RawPtr<IStreamBuffer> InputBuffer() const { return m_inputBuffer; }
    RawPtr<IStreamBuffer> InputBuffer(IStreamBuffer* inBuf) { std::swap(m_inputBuffer, inBuf); return inBuf; }
    RawPtr<OStreamBuffer> OutputBuffer() const { return m_outputBuffer; }
    RawPtr<OStreamBuffer> OutputBuffer(OStreamBuffer* outBuf) { std::swap(m_outputBuffer, outBuf); return outBuf; }
};

// Basic Input/Output Streams

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicIStream;

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicOStream;

template <typename CharT, typename Traits>
class BasicIStream: public virtual BasicIOS<CharT, Traits> {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
protected:
    BasicIStream() {}
public:
    BasicIStream(BasicIStreamBuffer<CharType, Traits>* inBuf) { this->InputBuffer(inBuf); }

    /// @brief Read a character from the input stream without removing it
    IntType PeekCh() { return this->InputBuffer()->SBumpC(); }
    /// @brief Read a character from the input stream and remove it
    IntType GetCh() { return this->InputBuffer()->SGetC(); }
    /// @brief Put back the last character read from the input stream, if possible
    IntType UngetCh(IntType c = Traits::eof()) { return this->InputBuffer()->Pbackfail(c); }
    /// @brief Get multiple characters from the input stream
    std::streamsize GetN(CharType* s, std::streamsize n) { return this->InputBuffer()->SGetN(s, n); }
    /// @brief Flush the output stream, ensuring all buffered output is written
    int Flush() { return this->InputBuffer()->Sync(); }
    /// @brief Seek to a specific position in the input stream
    PosType SeekPos(PosType pos) { return this->InputBuffer()->SeekPos(pos); }
    /// @brief Seek to a specific offset from the current position in the input stream
    PosType SeekOff(OffType off, IOSBase::SeekDir dir) { return this->InputBuffer()->SeekOff(off, dir); }

    /// Implement for high-level stream operations
    /// @brief Read a line from the input stream into a string
    std::basic_string<CharType, Traits> GetLine(CharType delimiter = '\n') {
        std::basic_string<CharType, Traits> line;
        IntType c;
        while ((c = Traits::to_char_type(this->GetCh())) != Traits::eof() && c != Traits::to_char_type(delimiter)) {
            line.push_back(c);
        }
        return line;
    }
};

template <typename CharT, typename Traits>
class BasicOStream: public virtual BasicIOS<CharT, Traits> {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
protected:
    BasicOStream() {}
public:
    BasicOStream(BasicOStreamBuffer<CharType, Traits>* outBuf) { this->OutputBuffer(outBuf); }

    /// @brief Write a character to the output stream
    IntType PutC(CharType c) { return this->OutputBuffer()->SPutC(Traits::to_int_type(c)); }
    /// @brief Write multiple characters to the output stream
    std::streamsize PutN(const CharType* s, std::streamsize n) { return this->OutputBuffer()->SPutN(s, n);}
    /// @brief Flush the output stream, ensuring all buffered output is written
    int Flush() { return this->OutputBuffer()->Sync(); }
    /// @brief Seek to a specific position in the output stream
    PosType SeekPos(PosType pos) { return this->OutputBuffer()->SeekPos(pos); }
    /// @brief Seek to a specific offset from the current position in the output stream
    PosType SeekOff(OffType off, IOSBase::SeekDir dir) { return this->OutputBuffer()->SeekOff(off, dir); }
};

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicIOStream: public BasicIStream<CharT, Traits>, public BasicOStream<CharT, Traits> {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
protected:
    BasicIOStream() {}
public:
    BasicIOStream(BasicIStreamBuffer<CharType, Traits>* inBuf, BasicOStreamBuffer<CharType, Traits>* outBuf)
        : BasicIStream<CharT, Traits>(inBuf), BasicOStream<CharT, Traits>(outBuf) {}

    /// @brief Synchronize the input and output buffers
    int Sync() { return BasicIStream<CharT, Traits>::Flush() || BasicOStream<CharT, Traits>::Flush(); }
};

// Stream operators

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicIStream<CharT, Traits>& operator>>(BasicIStream<CharT, Traits>& stream, CharT& c) {
    c = stream.GetCh();
    return stream;
}

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicIStream<CharT, Traits>& operator>>(BasicIStream<CharT, Traits>& stream, CharT* str) {
    CharT c;
    while ((c = stream.GetCh()) != Traits::eof() && !std::isspace(c)) {
        *str++ = c;
    }
    *str = '\0'; // Null-terminate the string
    return stream;
}

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicIStream<CharT, Traits>& operator>>(BasicIStream<CharT, Traits>& stream, std::basic_string<CharT, Traits>& str) {
    CharT c;
    str.clear();
    if (stream.PeekCh() != Traits::eof() && std::isspace(stream.PeekCh())) {
        stream.GetCh(); // Skip leading whitespace
    }
    while ((c = stream.GetCh()) != Traits::eof() && !std::isspace(c)) {
        str.push_back(c);
    }
    return stream;
}

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicOStream<CharT, Traits>& operator<<(BasicOStream<CharT, Traits>& stream, CharT c) {
    stream.PutC(c);
    return stream;
}

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicOStream<CharT, Traits>& operator<<(BasicOStream<CharT, Traits>& stream, const CharT* str) {
    stream.PutN(str, std::char_traits<CharT>::length(str));
    return stream;
}

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicOStream<CharT, Traits>& operator<<(BasicOStream<CharT, Traits>& stream, std::basic_string_view<CharT, Traits> str) {
    stream.PutN(str.data(), str.size());
    return stream;
}

template <IsNumber Num, typename Traits = std::char_traits<char>>
requires (!Same<Num, char>)
inline BasicOStream<char, Traits>& operator<<(BasicOStream<char, Traits>& stream, Num rhs) {
    return stream << std::to_string(rhs); // Convert to string and write to stream
}

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicOStream<CharT, Traits>& operator<<(BasicOStream<CharT, Traits>& stream, 
        BasicOStream<CharT, Traits>& (*func)(BasicOStream<CharT, Traits>&)) {
    return func(stream); // Call the function with the stream
}

// Stream methods

template <typename CharT, typename Traits = std::char_traits<CharT>>
inline BasicOStream<CharT, Traits>& EndLine(BasicOStream<CharT, Traits>& stream) {
    stream.PutC(Traits::to_char_type('\n'));
    stream.Flush();
    return stream;
}

// Stream redirectors

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicIStreamRedirector {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
private:
    BasicIStream<CharType, Traits>* target_inputStream;
    BasicIStreamBuffer<CharType, Traits>* cachedBuffer;
public:
    BasicIStreamRedirector(BasicIStream<CharType, Traits>* source, BasicIStream<CharType, Traits>* target)
        : target_inputStream(target) {
        cachedBuffer = target->InputBuffer(source->InputBuffer());
    }
    BasicIStreamRedirector(const BasicIStreamRedirector&) = delete; // Disable copy constructor
    BasicIStreamRedirector& operator=(const BasicIStreamRedirector&) = delete;

    ~BasicIStreamRedirector() {
        target_inputStream->InputBuffer(cachedBuffer);
    }
};

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicOStreamRedirector {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
private:
    BasicOStream<CharType, Traits>* target_outputStream;
    BasicOStreamBuffer<CharType, Traits>* cachedBuffer;
public:
    BasicOStreamRedirector(BasicOStream<CharType, Traits>* source, BasicOStream<CharType, Traits>* target)
        : target_outputStream(target) {
        cachedBuffer = target->OutputBuffer(source->OutputBuffer());
    }
    BasicOStreamRedirector(const BasicOStreamRedirector&) = delete; // Disable copy constructor
    BasicOStreamRedirector& operator=(const BasicOStreamRedirector&) = delete;
    ~BasicOStreamRedirector() {
        target_outputStream->OutputBuffer(cachedBuffer);
    }
};

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicIOStreamRedirector {
public:
    using CharType = CharT;
    using TraitsType = Traits;
    using IntType = typename Traits::int_type;
    using PosType = typename Traits::pos_type;
    using OffType = typename Traits::off_type;
private:
    BasicIOStream<CharType, Traits>* target_ioStream;
    BasicIStreamBuffer<CharType, Traits>* cachedInputBuffer;
    BasicOStreamBuffer<CharType, Traits>* cachedOutputBuffer;
public:
    BasicIOStreamRedirector(BasicIOStream<CharType, Traits>* source, BasicIOStream<CharType, Traits>* target)
        : target_ioStream(target) {
        cachedInputBuffer = target->InputBuffer(source->InputBuffer());
        cachedOutputBuffer = target->OutputBuffer(source->OutputBuffer());
    }
    BasicIOStreamRedirector(const BasicIOStreamRedirector&) = delete; // Disable copy
    BasicIOStreamRedirector& operator=(const BasicIOStreamRedirector&) = delete;
    ~BasicIOStreamRedirector() {
        target_ioStream->InputBuffer(cachedInputBuffer);
        target_ioStream->OutputBuffer(cachedOutputBuffer);
    }
};

// Type aliases for standard character streams

using IStream = BasicIStream<char>;
using OStream = BasicOStream<char>;
using IOStream = BasicIOStream<char>;
using IStreamRedirector = BasicIStreamRedirector<char>;
using OStreamRedirector = BasicOStreamRedirector<char>;
using IOStreamRedirector = BasicIOStreamRedirector<char>;

// Exceptions for stream operations
class StreamException : public Exception {};
class SeekOutOfRangeException : public StreamException {};
class PbackfailUnsupportedException : public StreamException {};

LCORE_NAMESPACE_END
