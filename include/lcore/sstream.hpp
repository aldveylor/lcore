#pragma once
#include "iostream.hpp"
#include "string.hpp"

LCORE_NAMESPACE_BEGIN

template <typename CharT, typename Traits = std::char_traits<CharT>>
class BasicIStringViewBuffer : public BasicIStreamBuffer<CharT, Traits> {
public:
    using StringViewType = std::basic_string_view<CharT, Traits>;

    BasicIStringViewBuffer(StringViewType str)
        : BasicIStreamBuffer<CharT, Traits>(BufferPointer<CharT>{str.data(), str.data(), str.data() + str.size()}) {}
    
    /// @brief Get the string from the buffer
    StringViewType str() const {
        return StringViewType(this->buffer.begin, this->buffer.end - this->buffer.begin);
    }
    // Default implementation will read from the buffer, do not override
};

template <typename CharT, typename Allocator = std::allocator<CharT>>
struct _BasicStringBufferData {
    Allocator _allocator;
    
    CharT* _data;
    std::size_t _size; // Allocated size of the buffer, not length of the string

    std::array<BufferPointer<CharT>*, 2> _onChanged = {nullptr}; // Pointers to the input and output buffers, when the data changes, the buffers will be notified
    std::size_t _onChangedCount = 0;

    _BasicStringBufferData(std::size_t length = 0)
        : _data(_allocator.allocate(length + 1)), _size(length + 1) {} 
    
    _BasicStringBufferData(std::basic_string_view<CharT> str)
        : _data(_allocator.allocate(str.length() + 1)), _size(str.length() + 1) {
        std::copy(str.data(), str.data() + str.length(), _data);
        _data[str.size()] = '\0'; // Null-terminate the string
    }

    ~_BasicStringBufferData() {
        if (_data) {
            _allocator.deallocate(_data, _size);
        }
    }
    _BasicStringBufferData(const _BasicStringBufferData&) = delete;
    _BasicStringBufferData& operator=(const _BasicStringBufferData&) = delete;
    _BasicStringBufferData(_BasicStringBufferData&& other) noexcept
        : _allocator(std::move(other._allocator)), _data(other._data), _size(other._size) {
        other._data = nullptr;
        other._size = 0;
    }

    void OnLengthChanged(std::size_t length) {
        for (auto pbuffer: _onChanged) {
            if (pbuffer) {
                pbuffer->end = _data + length;
            }
        }
    }

    void ReAlloc(std::size_t requiredSize = 0) {
        std::size_t newSize = std::max(_size * 2, requiredSize);
        CharT* newData = _allocator.allocate(newSize);
        std::copy(_data, _data + _size, newData);
        _allocator.deallocate(_data, _size);
        _data = newData;
        _size = newSize;

        for (auto pbuffer: _onChanged) {
            if (pbuffer) {
                std::size_t srcOff = pbuffer->current - pbuffer->begin;
                std::size_t srcSize = pbuffer->end - pbuffer->begin;
                pbuffer->begin = _data;
                pbuffer->current = _data + srcOff;
                pbuffer->end = _data + srcSize;
            }
        }
    }
};

// Forward declarations

template <typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
class BasicIStringStream;

template <typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
class BasicOStringStream;

template <typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
class BasicIOStringStream;

// String buffer classes for input and output streams

template <typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
class BasicIStringBuffer : public BasicIStreamBuffer<CharT, Traits> {
    friend class BasicIOStringStream<CharT, Traits, Allocator>;
public:
    using AllocatorType = Allocator;
    using StringType = std::basic_string<CharT, Traits, Allocator>;
    using StringViewType = std::basic_string_view<CharT, Traits>;

    using CharType = typename BasicIStreamBuffer<CharT, Traits>::CharType;
    using TraitsType = typename BasicIStreamBuffer<CharT, Traits>::TraitsType;
    using IntType = typename BasicIStreamBuffer<CharT, Traits>::IntType;
    using PosType = typename BasicIStreamBuffer<CharT, Traits>::PosType;
    using OffType = typename BasicIStreamBuffer<CharT, Traits>::OffType;
private:
    _BasicStringBufferData<CharT, Allocator>* _buffer;
public:
    BasicIStringBuffer(_BasicStringBufferData<CharT, Allocator>* bufferdata) : _buffer(bufferdata) {
        this->_buffer->_onChanged[this->_buffer->_onChangedCount++] = &this->buffer;
        this->buffer.begin = this->_buffer->_data;
        this->buffer.current = this->_buffer->_data;
        this->buffer.end = this->_buffer->_data + this->_buffer->_size - 1; // Leave space for null terminator
    }

    // Seek functions
    PosType SeekPos(PosType pos) override {
        if (pos < 0 || pos >= static_cast<PosType>(this->_buffer->_size)) {
            throw SeekOutOfRangeException(); // Out of bounds
        }
        this->buffer.current = this->buffer.begin + pos;
        return pos;
    }
    PosType SeekOff(OffType off, IOSBase::SeekDir dir) override {
        PosType newPos = 0;
        switch (dir) {
            case IOSBase::SeekDir::Begin:
                newPos = off;
                break;
            case IOSBase::SeekDir::Current:
                newPos = this->buffer.current - this->buffer.begin + off;
                break;
            case IOSBase::SeekDir::End:
                newPos = this->buffer.end - this->buffer.begin + off;
                break;
        }
        if (newPos < 0 || newPos >= static_cast<PosType>(this->buffer.end - this->buffer.begin)) {
            throw SeekOutOfRangeException(); // Out of bounds
        }
        this->buffer.current = this->buffer.begin + newPos;
        return newPos;
    }
    IntType Showmanyc() const override {
        return static_cast<IntType>(this->buffer.end - this->buffer.current);
    }
    
    StringViewType View() const {
        return StringViewType(this->buffer.begin, this->buffer.end);
    }
};

template <typename CharT, typename Traits = std::char_traits<CharT>, typename Allocator = std::allocator<CharT>>
class BasicOStringBuffer : public BasicOStreamBuffer<CharT, Traits> {
    friend class BasicIOStringStream<CharT, Traits, Allocator>;
public:
    using AllocatorType = Allocator;
    using StringType = std::basic_string<CharT, Traits, Allocator>;
    using StringViewType = std::basic_string_view<CharT, Traits>;
    
    using CharType = typename BasicOStreamBuffer<CharT, Traits>::CharType;
    using TraitsType = typename BasicOStreamBuffer<CharT, Traits>::TraitsType;
    using IntType = typename BasicOStreamBuffer<CharT, Traits>::IntType;
    using PosType = typename BasicOStreamBuffer<CharT, Traits>::PosType;
    using OffType = typename BasicOStreamBuffer<CharT, Traits>::OffType;
private:
    _BasicStringBufferData<CharT, Allocator>* _buffer;
public:
    BasicOStringBuffer(_BasicStringBufferData<CharT, Allocator>* bufferdata): _buffer(bufferdata) {
        this->_buffer->_onChanged[this->_buffer->_onChangedCount++] = &this->buffer;
        this->buffer.begin = this->_buffer->_data;
        this->buffer.current = this->_buffer->_data + this->_buffer->_size - 1;
        this->buffer.end = this->_buffer->_data + this->_buffer->_size - 1; // Leave space for null terminator
    }

    // Seek functions
    PosType SeekPos(PosType pos) override {
        if (pos < 0 || pos >= static_cast<PosType>(this->buffer.end - this->buffer.begin)) {
            throw SeekOutOfRangeException(); // Out of bounds
        }
        this->buffer.current = this->buffer.begin + pos;
        return pos;
    }
    PosType SeekOff(OffType off, IOSBase::SeekDir dir) override {
        PosType newPos = 0;
        switch (dir) {
            case IOSBase::SeekDir::Begin:
                newPos = off;
                break;
            case IOSBase::SeekDir::Current:
                newPos = this->buffer.current - this->buffer.begin + off;
                break;
            case IOSBase::SeekDir::End:
                newPos = this->buffer.end - this->buffer.begin + off;
                break;
        }
        if (newPos < 0 || newPos >= static_cast<PosType>(this->buffer.end - this->buffer.begin)) {
            throw SeekOutOfRangeException(); // Out of bounds
        }
        this->buffer.current = this->buffer.begin + newPos;
        return newPos;
    }
    std::streamsize SPutN(const CharType* s, std::streamsize n) override {
        if (this->buffer.current + n > this->buffer.end) {
            // Not enough space, reallocate
            this->_buffer->ReAlloc(this->buffer.current - this->buffer.begin + n + 1);
        }
        std::copy(s, s + n, this->buffer.current);
        this->buffer.current += n;
        if (this->buffer.current > this->buffer.end)
            this->_buffer->OnLengthChanged(this->buffer.current - this->buffer.begin);
        return n;
    }
protected:
    IntType Overflow(IntType c = Traits::eof()) override {
        this->_buffer->ReAlloc();
        if (c != Traits::eof()) {
            *this->buffer.current++ = Traits::to_char_type(c);
            this->_buffer->OnLengthChanged(this->buffer.current - this->buffer.begin);
        }
        return Traits::to_int_type(c);
    }
public:
    StringViewType View() const {
        return StringViewType(this->buffer.begin, this->buffer.end);
    }
    StringType Str() const {
        return StringType(this->buffer.begin, this->buffer.end);
    }
};

template <typename CharT, typename Traits, typename Allocator>
class BasicIStringStream : public BasicIStream<CharT, Traits> {
public:
    using StringType = std::basic_string<CharT, Traits, Allocator>;
    using StringViewType = std::basic_string_view<CharT, Traits>;
    using StringBufferType = BasicIStringBuffer<CharT, Traits, Allocator>;
private:
    _BasicStringBufferData<CharT, Allocator> _bufferdata; ///< Pointer to the string buffer data
    StringBufferType _buffer; ///< Input buffer for the string stream
public:
    BasicIStringStream(): _buffer(&_bufferdata) { this->InputBuffer(&_buffer); }
    BasicIStringStream(StringViewType str): _bufferdata(str.size()), _buffer(&_bufferdata) {
        std::copy(str.data(), str.data() + str.size(), this->_bufferdata._data);
        this->_bufferdata._data[str.size()] = '\0'; // Null-terminate the string
        this->InputBuffer(&_buffer);
    }

    /// @brief Get the string view from the input stream
    /// @note This method will not check if the buffer is @ref BasicIStringBuffer, if you manually set a different buffer, this method may not work as expected. 
    StringViewType View() const { return this->InputBuffer().template Cast<StringBufferType>()->View(); }
};

template <typename CharT, typename Traits, typename Allocator>
class BasicOStringStream : public BasicOStream<CharT, Traits> {
public:
    using StringType = std::basic_string<CharT, Traits, Allocator>;
    using StringViewType = std::basic_string_view<CharT, Traits>;
    using StringBufferType = BasicOStringBuffer<CharT, Traits, Allocator>;
private:
    _BasicStringBufferData<CharT, Allocator> _bufferdata; ///< Pointer to the string buffer data
    StringBufferType _buffer; ///< Output buffer for the string stream
public:
    BasicOStringStream(): _buffer(&_bufferdata) { this->OutputBuffer(&_buffer); }
    BasicOStringStream(StringViewType str): _bufferdata(str.size()), _buffer(&_bufferdata) {
        std::copy(str.data(), str.data() + str.size(), this->_bufferdata._data);
        this->_bufferdata._data[str.size()] = '\0'; // Null-terminate the string
        this->OutputBuffer(&_buffer);
    }

    /// @brief Get the string view from the output stream
    /// @note This method will not check if the buffer is @ref BasicOStringBuffer, if you manually set a different buffer, this method may not work as expected. 
    inline StringViewType View() const { return this->OutputBuffer().template Cast<StringBufferType>()->View(); }
    /// @brief Get the string from the output stream
    /// @note This method will not check if the buffer is @ref BasicOStringBuffer, if you manually set a different buffer, this method may not work as expected. 
    inline StringType Str() const { return this->OutputBuffer().template Cast<StringBufferType>()->Str(); }
};

template <typename CharT, typename Traits, typename Allocator>
class BasicIOStringStream : public BasicIOStream<CharT, Traits> {
public:
    using StringType = std::basic_string<CharT, Traits, Allocator>;
    using StringViewType = std::basic_string_view<CharT, Traits>;
    using IStringBufferType = BasicIStringBuffer<CharT, Traits, Allocator>;
    using OStringBufferType = BasicOStringBuffer<CharT, Traits, Allocator>;
private:
    _BasicStringBufferData<CharT, Allocator> _buffer; ///< Pointer to the string buffer
    IStringBufferType _inputBuffer; ///< Input buffer for the string stream
    OStringBufferType _outputBuffer; ///< Output buffer for the string stream
public:
    BasicIOStringStream(): _buffer(), _inputBuffer(&_buffer), _outputBuffer(&_buffer) {
        this->InputBuffer(&_inputBuffer);
        this->OutputBuffer(&_outputBuffer);
    }
    BasicIOStringStream(StringViewType str): _buffer(str.size()), _inputBuffer(&_buffer), _outputBuffer(&_buffer) {
        std::copy(str.data(), str.data() + str.size(), this->_buffer._data);
        this->_buffer._data[str.size()] = '\0'; // Null-terminate the string
        this->InputBuffer(&_inputBuffer);
        this->OutputBuffer(&_outputBuffer);
    }

    /// @brief Get the string view
    /// @note This method will not check if the buffer is @ref BasicIStringBuffer
    StringViewType View() const { return this->InputBuffer().template Cast<IStringBufferType>()->View(); } // Same in output buffer
    /// @brief Get the string
    /// @note This method will not check if the buffer is @ref BasicOStringBuffer
    StringType Str() const { return this->OutputBuffer().template Cast<OStringBufferType>()->Str(); }
};

using IStringStream = BasicIStringStream<char>;
using OStringStream = BasicOStringStream<char>;
using IOStringStream = BasicIOStringStream<char>;

LCORE_NAMESPACE_END
