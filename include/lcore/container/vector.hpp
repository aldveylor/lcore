#pragma once
#include "lcore/base.hpp"
#include "lcore/traits.hpp"
#include <vector>
#include <algorithm>

LCORE_NAMESPACE_BEGIN

template <typename T, typename Allocator = std::allocator<T>>
class Vector: public std::vector<T, Allocator> {
public:
    using std::vector<T, Allocator>::vector;
    using iterator = typename std::vector<T, Allocator>::iterator;
    using const_iterator = typename std::vector<T, Allocator>::const_iterator;
    using reserve_iterator = typename std::vector<T, Allocator>::reverse_iterator;
    using const_reserve_iterator = typename std::vector<T, Allocator>::const_reverse_iterator;

    using value_type = T;
    using reference = typename std::vector<T, Allocator>::reference;
    using const_reference = typename std::vector<T, Allocator>::const_reference;

    using size_type = typename std::vector<T, Allocator>::size_type;
    using difference_type = typename std::vector<T, Allocator>::difference_type;

    using allocator_type = Allocator;

    template <Iterable tContainer>
    requires Same<T, IterableValueType<tContainer>>
    inline Vector(const tContainer& container): std::vector<T, Allocator>::vector(container.begin(), container.end()) {};

    inline bool contains(const T& value) const {
        return std::find(std::begin(*this), std::end(*this), value) != std::end(*this);
    }

    template <typename Func>
    requires IsCallable<Func, const T&> && Same<ResultCallable<Func, const T&>, bool>
    inline bool contains(Func f) const {
        return std::find_if(std::begin(*this), std::end(*this), f) != std::end(*this);
    }

    inline void insert_unique(const T& value) {
        if (!contains(value)) {
            this->push_back(value);
        }
    }

    template <typename Func>
    requires IsCallable<Func, const T&> && Same<ResultCallable<Func, const T&>, bool>
    inline void remove_if(Func f){
        std::erase(std::remove_if(std::begin(*this), std::end(*this), f), std::end(*this));
    }

    template <Iterable tContainer>
    requires Same<IterableValueType<tContainer>, T>
    inline void extends(const tContainer& other){
        this->insert(std::end(*this), std::begin(other), std::end(other));
    }

    template <Iterable tContainer, typename Func>
    requires Same<T, ResultCallable<Func, const IterableValueType<tContainer>&>>
    inline static Vector<T> fromContainer(const tContainer& iterable, Func f) {
        Vector<T> res;
        if constexpr (IsInteger<decltype(std::declval<tContainer>().size())>) {
            res.reserve(iterable.size());
        }
        for (auto& item: iterable) {
            res.push_back(f(item));
        }
        return res;
    }
};

LCORE_NAMESPACE_END
