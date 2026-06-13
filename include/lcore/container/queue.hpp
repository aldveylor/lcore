#pragma once
#include "lcore/base.hpp"
#include <queue>

LCORE_NAMESPACE_BEGIN

template<class T,
         class Container = std::vector<T>,
         class Compare = std::less<T>>
class IterablePriorityQueue 
    : public std::priority_queue<T, Container, Compare>
{
public:
    using Base = std::priority_queue<T, Container, Compare>;

    const Container& data() const
    {
        return Base::c;
    }

    Container& data()
    {
        return Base::c;
    }
};

LCORE_NAMESPACE_END
