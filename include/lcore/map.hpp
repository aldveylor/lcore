#pragma once
#include "base.hpp"
#include "traits.hpp"
#include <map>
#include <functional>
#include <initializer_list>
#include <stdexcept>

LCORE_NAMESPACE_BEGIN

/// @brief Map is a simple key-value map
/// @tparam K Key type
/// @tparam V Value type
/// @tparam Compare Compare function
/// @tparam Allocator Alloctor type
template <typename K, typename V, typename Compare = std::less<K>, typename Allocator = std::allocator<std::pair<const K,V>>>
using Map = std::map<K, V, Compare, Allocator>;

/// @brief BiMap is a bidirectional map
/// @tparam L Left type
/// @tparam R Right type
/// @tparam CompareL Left compare function
/// @tparam CompareR Right compare function
/// @tparam allocator Allocator type
template <typename L, typename R, typename CompareL = std::less<L>, typename CompareR = std::less<R>, 
        template <typename T> typename allocator = std::allocator>
class Bimap {
public:
    using LAllocator = allocator<std::pair<const L, R>>;
    using RAllocator = allocator<std::pair<const R, L>>;

    using LeftMap = Map<L, R, CompareL, LAllocator>;
    using RightMap = Map<R, L, CompareR, RAllocator>;
private:
    LeftMap left;
    RightMap right;
public:
    void insert(const L& l, const R& r){
        left.insert({l, r});
        right.insert({r, l});
    }

    const R& getRight(const L& l) const {
        return left.at(l);
    }

    const L& getLeft(const R& r) const {
        return right.at(r);
    }

    const LeftMap& getLeftMap() const {
        return left;
    }

    const RightMap& getRightMap() const {
        return right;
    }

    void clear(){
        left.clear();
        right.clear();
    }

    Bimap() = default;
    Bimap(const Bimap&) = default;
    Bimap(Bimap&&) = default;

    Bimap& operator=(const Bimap&) = default;

    Bimap(std::initializer_list<std::pair<L, R>> list){
        for (auto& [l, r]: list){
            insert(l, r);
        }
    }

    Bimap(const std::map<L, R>& lmap){
        for (auto& [l, r]: lmap){
            insert(l, r);
        }
    }
};

template <typename T>
class WeakPtr;

template <typename K, typename V, typename Compare = std::less<K>, 
    template <typename> typename Weak = WeakPtr, typename Allocator = std::allocator<std::pair<const K, Weak<V>>>>
class WeakMap: protected Map<K, Weak<V>, Compare, Allocator> {
public:
    using Base = Map<K, Weak<V>, Compare, Allocator>;
    using typename Base::key_type;
    using typename Base::value_type;
    using typename Base::mapped_type;
    // using shared_value_type = decltype(std::declval<mapped_type>().Lock());

    using Base::Base; // Inherit constructors

    class iterator {
        friend class WeakMap<K, V, Compare, Weak, Allocator>;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename Base::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

    private:
        Base* base;
        typename Base::iterator it;

    public:
        iterator() = default;
        iterator(Base* base, typename Base::iterator it): base(base), it(it) {}

        reference operator*() { return *it; }
        pointer operator->() { return &(*it); }

        iterator& operator++() {
            ++it;
            while (it != base->end() && it->second.Expired()) {
                ++it; // Skip expired weak pointers
            }
            return *this;
        }

        iterator operator++(int) {
            iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const iterator& other) const { return it == other.it; }
        bool operator!=(const iterator& other) const { return it != other.it; }
    };
    class const_iterator {
        friend class WeakMap<K, V, Compare, Weak, Allocator>;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename Base::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

    private:
        const Base* base;
        typename Base::const_iterator it;

    public:
        const_iterator() = default;
        const_iterator(const Base* base, typename Base::const_iterator it): base(base), it(it) {}
        const_iterator(const iterator& other): base(other.base), it(other.it) {}

        reference operator*() const { return *it; }
        pointer operator->() const { return &(*it); }

        const_iterator& operator++() {
            ++it;
            while (it != base->end() && it->second.Expired()) {
                ++it; // Skip expired weak pointers
            }
            return *this;
        }

        const_iterator operator++(int) {
            const_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_iterator& other) const { return it == other.it; }
        bool operator!=(const const_iterator& other) const { return it != other.it; }
    };
    class reverse_iterator {
        friend class WeakMap<K, V, Compare, Weak, Allocator>;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename Base::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = value_type*;
        using reference = value_type&;

    private:
        Base* base;
        typename Base::reverse_iterator it;

    public:
        reverse_iterator() = default;
        reverse_iterator(Base* base, typename Base::reverse_iterator it): base(base), it(it) {}

        reference operator*() { return *it; }
        pointer operator->() { return &(*it); }

        reverse_iterator& operator++() {
            ++it;
            while (it != base->rend() && it->second.Expired()) {
                ++it; // Skip expired weak pointers
            }
            return this;
        }

        reverse_iterator operator++(int) {
            reverse_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const reverse_iterator& other) const { return it == other.it; }
        bool operator!=(const reverse_iterator& other) const { return it != other.it; }
    };
    class const_reverse_iterator {
        friend class WeakMap<K, V, Compare, Weak, Allocator>;
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = typename Base::value_type;
        using difference_type = std::ptrdiff_t;
        using pointer = const value_type*;
        using reference = const value_type&;

    private:
        const Base* base;
        typename Base::const_reverse_iterator it;

    public:
        const_reverse_iterator() = default;
        const_reverse_iterator(const Base* base, typename Base::const_reverse_iterator it): base(base), it(it) {}
        const_reverse_iterator(const reverse_iterator& other): base(other.base), it(other.it) {}

        reference operator*() const { return *it; }
        pointer operator->() const { return &(*it); }

        const_reverse_iterator& operator++() {
            ++it;
            while (it != base->rend() && it->second.Expired()) {
                ++it; // Skip expired weak pointers
            }
            return this;
        }

        const_reverse_iterator operator++(int) {
            const_reverse_iterator tmp = *this;
            ++(*this);
            return tmp;
        }

        bool operator==(const const_reverse_iterator& other) const { return it == other.it; }
        bool operator!=(const const_reverse_iterator& other) const { return it != other.it; }
    };

    iterator begin() {
        auto ret = iterator(this, Base::begin());
        if (ret.it != Base::end() && ret.it->second.Expired()) {
            ++ret; // Skip expired weak pointers
        }
        return ret;
    }
    const_iterator begin() const {
        auto ret = const_iterator(this, Base::begin());
        if (ret.it != Base::end() && ret.it->second.Expired()) {
            ++ret; // Skip expired weak pointers
        }
        return ret;
    }
    iterator end() {
        return iterator(this, Base::end());
    }
    const_iterator end() const {
        return const_iterator(this, Base::end());
    }
    reverse_iterator rbegin() {
        auto ret = reverse_iterator(this, Base::rbegin());
        if (ret.it != Base::rend() && ret.it->second.Expired()) {
            ++ret; // Skip expired weak pointers
        }
        return ret;
    }
    const_reverse_iterator rbegin() const {
        auto ret = const_reverse_iterator(this, Base::rbegin());
        if (ret.it != Base::rend() && ret.it->second.Expired()) {
            ++ret; // Skip expired weak pointers
        }
        return ret;
    }
    reverse_iterator rend() {
        return reverse_iterator(this, Base::rend());
    }
    const_reverse_iterator rend() const {
        return const_reverse_iterator(this, Base::rend());
    }

    const_iterator cbegin() const { return begin(); }
    const_iterator cend() const { return end(); }
    const_reverse_iterator crbegin() const { return rbegin(); }
    const_reverse_iterator crend() const { return rend(); }

    void clear_expired() {
        for (auto it = Base::begin(); it != Base::end();) {
            if (it->second.Expired()) {
                it = Base::erase(it); // Remove expired weak pointers
            } else {
                ++it;
            }
        }
    }

    bool empty() const {
        return Base::empty() || std::all_of(Base::begin(), Base::end(), [](const auto& pair) {
            return pair.second.Expired();
        });
    }
    size_t size() const {
        return std::count_if(Base::begin(), Base::end(), [](const auto& pair) {
            return !pair.second.Expired();
        });
    }

    std::pair<iterator, bool> insert(const value_type& value) {
        auto [it, success] = Base::insert(value);
        if (success) {
            return {iterator(this, it), true};
        }
        // Not successful
        if (it->second.Expired()) {
            it->second = value.second; // Update the weak pointer if it was expired
            return {iterator(this, it), true};
        }
        return {iterator(this, it), false}; // Return existing iterator if not expired
    }

    iterator erase(iterator pos) {
        if (pos.it != Base::end()) {
            auto ret = Base::erase(pos.it);
            return iterator(this, ret);
        }
        return end();
    }
    iterator erase(const_iterator pos) {
        if (pos.it != Base::end()) {
            auto ret = Base::erase(pos.it);
            return iterator(this, ret);
        }
        return end();
    }
    size_t erase(const key_type& key) {
        auto it = Base::find(key);
        if (it != Base::end()) {
            bool expired = it->second.Expired();
            Base::erase(it);
            return expired;
        }
        return 0;
    }

    iterator find(const key_type& key) {
        auto it = Base::find(key);
        if (it != Base::end() && it->second.Expired()) {
            return end(); // Return end if the weak pointer is expired
        }
        return iterator(this, it);
    }
    const_iterator find(const key_type& key) const {
        auto it = Base::find(key);
        if (it != Base::end() && it->second.Expired()) {
            return end(); // Return end if the weak pointer is expired
        }
        return const_iterator(this, it);
    }

    size_t count(const key_type& key) const {
        auto it = Base::find(key);
        if (it != Base::end() && !it->second.Expired()) {
            return 1; // Return 1 if the weak pointer is valid
        }
        return 0; // Return 0 if the weak pointer is expired or not found
    }

    mapped_type& at(const key_type& key) {
        auto it = Base::find(key);
        if (it != Base::end() && !it->second.Expired()) {
            return it;
        }
        throw std::out_of_range("Key not found or weak pointer expired");
    }
    const mapped_type& at(const key_type& key) const {
        auto it = Base::find(key);
        if (it != Base::end() && !it->second.Expired()) {
            return it;
        }
        throw std::out_of_range("Key not found or weak pointer expired");
    }
    bool contains(const key_type& key) const {
        auto it = Base::find(key);
        return it != Base::end() && !it->second.Expired();
    }

    // operators
    using Base::operator=;
    
    mapped_type& operator[](const key_type& key) {
        auto it = Base::find(key);
        if (it != Base::end()) {
            if (it->second.Expired()) {
                it->second.Reset(); // Reset the weak pointer if it was expired
            }
            return it->second; // Return the existing weak pointer
        }
        // If not found or expired, insert a new weak pointer
        auto [new_it, success] = Base::insert({key, Weak<V>()});
        if (success) {
            return new_it->second; // Return the newly inserted weak pointer
        }
        // If insertion failed, return the existing weak pointer
        return it->second;
    }

};

LCORE_NAMESPACE_END
