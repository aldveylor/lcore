#pragma once
#include "lcore/base.hpp"
#include "lcore/traits.hpp"
#include "lcore/assert.hpp"
#include <vector>
#include <algorithm>
#include <cstdint>

namespace std {

template <typename T,  std::size_t N>
class array;

}

LCORE_NAMESPACE_BEGIN


/**
 * @brief Container View
 * Use this class to create a view of a container,
 * this view can be used to slice, split, and iterate over the container
 * @tparam Container Container type
 * @tparam T Value type
 */
template <template <typename> typename Container, typename T>
requires Iterable<Container<T>>
class ContainerView {
public:
    using value_type = T;
    using iterator = typename Container<T>::iterator;
    using const_iterator = typename Container<T>::const_iterator;
    using reverse_iterator = typename Container<T>::reverse_iterator;
    using const_reverse_iterator = typename Container<T>::const_reverse_iterator;

    using view_type = ContainerView<Container, T>;
private:
    iterator begin_;
    iterator end_;
public:
    inline ContainerView(Container<T>& container): begin_(container.begin()), end_(container.end()){};
    inline ContainerView(iterator begin, iterator end): begin_(begin), end_(end){};

    inline iterator begin() { return begin_; }
    inline iterator end() { return end_; }
    inline const_iterator begin() const { return begin_; }
    inline const_iterator end() const { return end_; }
    inline const_iterator cbegin() const { return begin_; }
    inline const_iterator cend() const { return end_; }
    inline reverse_iterator rbegin() { return reverse_iterator(end_); }
    inline reverse_iterator rend() { return reverse_iterator(begin_); }
    inline const_reverse_iterator rbegin() const { return const_reverse_iterator(end_); }
    inline const_reverse_iterator rend() const { return const_reverse_iterator(begin_); }
    inline const_reverse_iterator crbegin() const { return const_reverse_iterator(end_); }
    inline const_reverse_iterator crend() const { return const_reverse_iterator(begin_); }

    inline size_t size() const { return std::distance(begin_, end_); }
    inline bool empty() const { return begin_ == end_; }

    inline T& front() { return *begin_; }
    inline T& back() { return *std::prev(end_); }

    inline const T& front() const { return *begin_; }
    inline const T& back() const { return *std::prev(end_); }

    inline void pop_front() { begin_++; }
    inline void pop_back() { end_--; }

    inline operator Container<T>() const{
        return Container<T>(begin_, end_);
    }

    inline T& operator[](size_t index){
        return *std::next(begin_, index);
    }

    inline const T& operator[](size_t index) const {
        return *std::next(begin_, index);
    }

    inline view_type slice(size_t start, size_t end) const {
        return view_type(std::next(begin_, start), std::prev(begin_, end));
    }

    inline view_type slice(size_t start) const{
        return view_type(std::next(begin_, start), end_);
    }

    inline view_type subview(size_t start, size_t n = (size_t)-1) const {
        if (n == (size_t)-1) return view_type(std::next(begin_, start), end_);
        return view_type(std::next(begin_, start), std::next(begin_, start + n));
    }
    
    template <typename Func>
    requires IsCallable<Func, T&> && Same<ResultCallable<Func, T&>, bool>
    inline std::vector<view_type> split(Func f) {
        std::vector<view_type> result;
        auto it = begin_;
        while(it != end_){
            auto next = std::find_if(it, end_, f);
            if(next != end_){
                result.push_back(view_type(it, next));
                it = std::next(next);
            } else {
                result.push_back(view_type(it, end_));
                break;
            }
        }
        return result;
    }
};

/**
 * @brief Const Container View
 * This view cannot modify the container
 * @tparam Container Container type
 * @tparam T Value type
 */
template <template <typename> typename Container, typename T>
requires Iterable<Container<T>>
class ConstContainerView {
public:
    using value_type = T;
    using iterator = typename Container<T>::const_iterator;
    using const_iterator = typename Container<T>::const_iterator;
    using reverse_iterator = typename Container<T>::const_reverse_iterator;
    using const_reverse_iterator = typename Container<T>::const_reverse_iterator;

    using view_type = ConstContainerView<Container, T>;
private:
    const_iterator begin_;
    const_iterator end_;

public:
    inline ConstContainerView(const Container<T>& container): begin_(container.begin()), end_(container.end()){};
    inline ConstContainerView(const_iterator begin, const_iterator end): begin_(begin), end_(end){};

    // inline iterator begin() { return begin_; }
    // inline iterator end() { return end_; }
    inline const_iterator begin() const { return begin_; }
    inline const_iterator end() const { return end_; }
    inline const_iterator cbegin() const { return begin_; }
    inline const_iterator cend() const { return end_; }
    // inline reverse_iterator rbegin() { return reverse_iterator(end_); }
    // inline reverse_iterator rend() { return reverse_iterator(begin_); }
    inline const_reverse_iterator rbegin() const { return const_reverse_iterator(end_); }
    inline const_reverse_iterator rend() const { return const_reverse_iterator(begin_); }
    inline const_reverse_iterator crbegin() const { return const_reverse_iterator(end_); }
    inline const_reverse_iterator crend() const { return const_reverse_iterator(begin_); }

    inline size_t size() const { return std::distance(begin_, end_); }
    inline bool empty() const { return begin_ == end_; }

    // inline T& front() { return *begin_; }
    // inline T& back() { return *std::prev(end_); }

    inline const T& front() const { return *begin_; }
    inline const T& back() const { return *std::prev(end_); }

    inline void pop_front() { begin_++; }
    inline void pop_back() { end_--; }

    inline operator Container<T>() const{
        return Container<T>(begin_, end_);
    }

    // inline T& operator[](size_t index){
    //     return *std::next(begin_, index);
    // }

    inline const T& operator[](size_t index) const {
        return *std::next(begin_, index);
    }

    inline view_type slice(size_t start, size_t end) const {
        return view_type(std::next(begin_, start), std::prev(begin_, end));
    }

    inline view_type slice(size_t start) const{
        return view_type(std::next(begin_, start), end_);
    }

    inline view_type subview(size_t start, size_t n = (size_t)-1) const {
        if (n == (size_t)-1) return view_type(std::next(begin_, start), end_);
        return view_type(std::next(begin_, start), std::next(begin_, start + n));
    }
    
    template <typename Func>
    requires IsCallable<Func, const T&> && Same<ResultCallable<Func, const T&>, bool>
    inline std::vector<view_type> split(Func f) const {
        std::vector<view_type> result;
        auto it = begin_;
        while(it != end_){
            auto next = std::find_if(it, end_, f);
            if(next != end_){
                result.push_back(view_type(it, next));
                it = std::next(next);
            } else {
                result.push_back(view_type(it, end_));
                break;
            }
        }
        return result;
    }
};

template <typename T>
class Span {
public:
    using value_type = T;
    using iterator = T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<T*>;
    using const_reverse_iterator = std::reverse_iterator<const T*>;

    using view_type = Span<T>;
private:
    iterator begin_;
    iterator end_;
public:
    inline constexpr Span(T* begin, T* end): begin_(begin), end_(end){};
    inline constexpr Span(T* begin, size_t count): begin_(begin), end_(begin + count){};
    template <std::size_t N>
    inline constexpr Span(T (&arr)[N]): begin_(arr), end_(arr + N){};
    template <std::size_t N>
    inline constexpr Span(std::array<T, N>& arr): begin_(arr.data()), end_(arr.data() + N){};

    inline constexpr iterator begin() { return begin_; }
    inline constexpr iterator end() { return end_; }
    inline constexpr const_iterator begin() const { return begin_; }
    inline constexpr const_iterator end() const { return end_; }
    inline constexpr const_iterator cbegin() const { return begin_; }
    inline constexpr const_iterator cend() const { return end_; }
    inline constexpr reverse_iterator rbegin() { return reverse_iterator(end_); }
    inline constexpr reverse_iterator rend() { return reverse_iterator(begin_); }
    inline constexpr const_reverse_iterator rbegin() const { return const_reverse_iterator(end_); }
    inline constexpr const_reverse_iterator rend() const { return const_reverse_iterator(begin_); }
    inline constexpr const_reverse_iterator crbegin() const { return const_reverse_iterator(end_); }
    inline constexpr const_reverse_iterator crend() const { return const_reverse_iterator(begin_); }

    inline constexpr size_t size() const { return std::distance(begin_, end_); }
    inline constexpr bool empty() const { return begin_ == end_; }

    inline constexpr T& front() { return *begin_; }
    inline constexpr T& back() { return *std::prev(end_); }

    inline constexpr const T& front() const { return *begin_; }
    inline constexpr const T& back() const { return *std::prev(end_); }

    inline constexpr void pop_front() { begin_++; }
    inline constexpr void pop_back() { end_--; }

    inline operator std::vector<T>() const{
        return std::vector<T>(begin_, end_);
    }

    inline constexpr T& operator[](size_t index){
        return *std::next(begin_, index);
    }

    inline constexpr const T& operator[](size_t index) const {
        return *std::next(begin_, index);
    }

    inline constexpr view_type slice(size_t start, size_t end) const {
        return view_type(std::next(begin_, start), std::prev(begin_, end));
    }

    inline constexpr view_type slice(size_t start) const{
        return view_type(std::next(begin_, start), end_);
    }

    inline constexpr view_type subview(size_t start, size_t n = (size_t)-1) const {
        if (n == (size_t)-1) return view_type(std::next(begin_, start), end_);
        return view_type(std::next(begin_, start), std::next(begin_, start + n));
    }

    template <typename Func>
    requires IsCallable<Func, T&> && Same<ResultCallable<Func, T&>, bool>
    inline std::vector<view_type> split(Func f) {
        std::vector<view_type> result;
        auto it = begin_;
        while(it != end_){
            auto next = std::find_if(it, end_, f);
            if(next != end_){
                result.push_back(view_type(it, next));
                it = std::next(next);
            } else {
                result.push_back(view_type(it, end_));
                break;
            }
        }
        return result;
    }
};

template <typename T>
class Span<const T> {
public:
    using value_type = const T;
    using iterator = const T*;
    using const_iterator = const T*;
    using reverse_iterator = std::reverse_iterator<const T*>;
    using const_reverse_iterator = std::reverse_iterator<const T*>;

    using view_type = Span<const T>;
private:
    iterator begin_;
    iterator end_;
public:
    inline constexpr Span(const T* begin, const T* end): begin_(begin), end_(end){};
    inline constexpr Span(const T* begin, size_t count): begin_(begin), end_(begin + count){};
    template <std::size_t N>
    inline constexpr Span(const T (&arr)[N]): begin_(arr), end_(arr + N){};
    template <std::size_t N>
    inline constexpr Span(const std::array<T, N>& arr): begin_(arr.data()), end_(arr.data() + N){};

    // inline constexpr iterator begin() { return begin_; }
    // inline constexpr iterator end() { return end_; }
    inline constexpr const_iterator begin() const { return begin_; }
    inline constexpr const_iterator end() const { return end_; }
    inline constexpr const_iterator cbegin() const { return begin_; }
    inline constexpr const_iterator cend() const { return end_; }
    // inline constexpr reverse_iterator rbegin() { return reverse_iterator(end_); }
    // inline constexpr reverse_iterator rend() { return reverse_iterator(begin_); }
    inline constexpr const_reverse_iterator rbegin() const { return const_reverse_iterator(end_); }
    inline constexpr const_reverse_iterator rend() const { return const_reverse_iterator(begin_); }
    inline constexpr const_reverse_iterator crbegin() const { return const_reverse_iterator(end_); }
    inline constexpr const_reverse_iterator crend() const { return const_reverse_iterator(begin_); }

    inline constexpr size_t size() const { return std::distance(begin_, end_); }
    inline constexpr bool empty() const { return begin_ == end_; }

    // inline constexpr T& front() { return *begin_; }
    // inline constexpr T& back() { return *std::prev(end_); }

    inline constexpr const T& front() const { return *begin_; }
    inline constexpr const T& back() const { return *std::prev(end_); }

    inline constexpr void pop_front() { begin_++; }
    inline constexpr void pop_back() { end_--; }

    inline operator std::vector<T>() const{
        return std::vector<T>(begin_, end_);
    }

    // inline constexpr T& operator[](size_t index){
    //     return *std::next(begin_, index);
    // }

    inline constexpr const T& operator[](size_t index) const {
        return *std::next(begin_, index);
    }

    inline constexpr view_type slice(size_t start, size_t end) const {
        return view_type(std::next(begin_, start), std::prev(begin_, end));
    }

    inline constexpr view_type slice(size_t start) const{
        return view_type(std::next(begin_, start), end_);
    }

    inline constexpr view_type subview(size_t start, size_t n = (size_t)-1) const {
        if (n == (size_t)-1) return view_type(std::next(begin_, start), end_);
        return view_type(std::next(begin_, start), std::next(begin_, start + n));
    }

    template <typename Func>
    requires IsCallable<Func, const T&> && Same<ResultCallable<Func, const T&>, bool>
    inline std::vector<view_type> split(Func f) const {
        std::vector<view_type> result;
        auto it = begin_;
        while(it != end_){
            auto next = std::find_if(it, end_, f);
            if(next != end_){
                result.push_back(view_type(it, next));
                it = std::next(next);
            } else {
                result.push_back(view_type(it, end_));
                break;
            }
        }
        return result;
    }
};

/// @brief Strided Span
/// A span that allows iteration with a fixed stride,
/// which is useful for accessing elements in a non-contiguous manner.
template <typename T>
class StridedSpan {
    T* begin_;
    T* end_;
    size_t stride_;
public:
    class iterator {
        T* ptr_;
        size_t stride_;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;
        
        inline iterator(T* ptr, size_t stride): ptr_(ptr), stride_(stride) {}
        inline T& operator*() { return *ptr_; }
        inline iterator& operator++() { ptr_ = reinterpret_cast<T*>(reinterpret_cast<char*>(ptr_) + stride_); return *this; }
        inline iterator operator++(int) { iterator tmp = *this; tmp.operator++(); return tmp; }
        inline bool operator==(const iterator& other) const { return ptr_ == other.ptr_; }
        inline bool operator!=(const iterator& other) const { return ptr_ != other.ptr_; }
    };
    class const_iterator {
        const T* ptr_;
        size_t stride_;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        inline const_iterator(const T* ptr, size_t stride): ptr_(ptr), stride_(stride) {}
        inline const T& operator*() const { return *ptr_; }
        inline const_iterator& operator++() { ptr_ = reinterpret_cast<const T*>(reinterpret_cast<const char*>(ptr_) + stride_); return *this; }
        inline const_iterator operator++(int) { const_iterator tmp = *this; tmp.operator++(); return tmp; }
        inline bool operator==(const const_iterator& other) const { return ptr_ == other.ptr_; }
        inline bool operator!=(const const_iterator& other) const { return ptr_ != other.ptr_; }
    };
    class reverse_iterator {
        T* ptr_;
        size_t stride_;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using pointer = T*;
        using reference = T&;

        inline reverse_iterator(T* ptr, size_t stride): ptr_(ptr), stride_(stride) {}
        inline T& operator*() { return *ptr_; }
        inline reverse_iterator& operator++() { ptr_ = reinterpret_cast<T*>(reinterpret_cast<char*>(ptr_) - stride_); return *this; }
        inline reverse_iterator operator++(int) { reverse_iterator tmp = *this; tmp.operator++(); return tmp; }
        inline bool operator==(const reverse_iterator& other) const { return ptr_ == other.ptr_; }
        inline bool operator!=(const reverse_iterator& other) const { return ptr_ != other.ptr_; }
    };
    class const_reverse_iterator {
        const T* ptr_;
        size_t stride_;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = const T;
        using difference_type = std::ptrdiff_t;
        using pointer = const T*;
        using reference = const T&;

        inline const_reverse_iterator(const T* ptr, size_t stride): ptr_(ptr), stride_(stride) {}
        inline const T& operator*() const { return *ptr_; }
        inline const_reverse_iterator& operator++() { ptr_ = reinterpret_cast<const T*>(reinterpret_cast<const char*>(ptr_) - stride_); return *this; }
        inline const_reverse_iterator operator++(int) { const_reverse_iterator tmp = *this; tmp.operator++(); return tmp; }
        inline bool operator==(const const_reverse_iterator& other) const { return ptr_ == other.ptr_; }
        inline bool operator!=(const const_reverse_iterator& other) const { return ptr_ != other.ptr_; }
    };
public:
    using value_type = T;
    using view_type = StridedSpan<T>;

    inline constexpr StridedSpan(T* begin, T* end, size_t stride): begin_(begin), end_(end), stride_(stride) {
        LCORE_ASSERT(stride_ > 0, "Stride must be greater than 0");
        LCORE_ASSERT((reinterpret_cast<uintptr_t>(end_) - reinterpret_cast<uintptr_t>(begin_)) % stride_ == 0, "Invalid stride for the given range");
    }
    inline constexpr StridedSpan(T* begin, size_t count, size_t stride): begin_(begin), end_(reinterpret_cast<T*>(reinterpret_cast<char*>(begin) + count * stride)), stride_(stride) {
        LCORE_ASSERT(stride_ > 0, "Stride must be greater than 0");
    }

    inline constexpr iterator begin() { return iterator(begin_, stride_); }
    inline constexpr iterator end() { return iterator(end_, stride_); }
    inline constexpr const_iterator begin() const { return const_iterator(begin_, stride_); }
    inline constexpr const_iterator end() const { return const_iterator(end_, stride_); }
    inline constexpr const_iterator cbegin() const { return const_iterator(begin_, stride_); }
    inline constexpr const_iterator cend() const { return const_iterator(end_, stride_); }
    inline constexpr reverse_iterator rbegin() { return reverse_iterator(std::prev(end_, stride_), stride_); }
    inline constexpr reverse_iterator rend() { return reverse_iterator(std::prev(begin_, stride_), stride_); }
    inline constexpr const_reverse_iterator rbegin() const { return const_reverse_iterator(std::prev(end_, stride_), stride_); }
    inline constexpr const_reverse_iterator rend() const { return const_reverse_iterator(std::prev(begin_, stride_), stride_); }
    inline constexpr const_reverse_iterator crbegin() const { return const_reverse_iterator(std::prev(end_, stride_), stride_); }
    inline constexpr const_reverse_iterator crend() const { return const_reverse_iterator(std::prev(begin_, stride_), stride_); }

    inline constexpr size_t size() const { return (reinterpret_cast<const char*>(end_) - reinterpret_cast<const char*>(begin_)) / stride_; }
    inline constexpr bool empty() const { return begin_ == end_; }
    
    inline constexpr T& front() { return *begin_; }
    inline constexpr T& back() { return *reinterpret_cast<T*>(reinterpret_cast<char*>(end_) - stride_); }
    inline constexpr const T& front() const { return *begin_; }
    inline constexpr const T& back() const { return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(end_) - stride_); }

    inline constexpr void pop_front() { reinterpret_cast<char*&>(begin_) += stride_; }
    inline constexpr void pop_back() { reinterpret_cast<char*&>(end_) -= stride_; }

    inline operator std::vector<T>() const{
        std::vector<T> result;
        for(auto it = begin_; it != end_; reinterpret_cast<char*&>(it) += stride_){
            result.push_back(*it);
        }
        return result;
    }
    inline constexpr T& operator[](size_t index){
        return *reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + index * stride_);
    }
    inline constexpr const T& operator[](size_t index) const {
        return *reinterpret_cast<const T*>(reinterpret_cast<const char*>(begin_) + index * stride_);
    }
    inline constexpr view_type slice(size_t start, size_t end) const {
        return view_type(reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + start * stride_), reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + end * stride_), stride_);
    }
    inline constexpr view_type slice(size_t start) const{
        return view_type(reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + start * stride_), end_, stride_);
    }
    inline constexpr view_type subview(size_t start, size_t n = (size_t)-1) const {
        if (n == (size_t)-1) return view_type(reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + start * stride_), end_, stride_);
        return view_type(reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + start * stride_), reinterpret_cast<T*>(reinterpret_cast<char*>(begin_) + (start + n) * stride_), stride_);
    }
    template <typename Func>
    requires IsCallable<Func, T&> && Same<ResultCallable<Func, T&>, bool>
    inline std::vector<view_type> split(Func f) {
        std::vector<view_type> result;
        auto it = begin_;
        while(it != end_){
            auto next = std::find_if(it, end_, f);
            if(next != end_){
                result.push_back(view_type(it, next, stride_));
                it = std::next(next);
            } else {
                result.push_back(view_type(it, end_, stride_));
                break;
            }
        }
        return result;
    }
};

LCORE_NAMESPACE_END
