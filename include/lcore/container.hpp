#pragma once
#include "base.hpp"

#include "container/vector.hpp"
#include "container/list.hpp"
#include "container/view.hpp"

LCORE_NAMESPACE_BEGIN

namespace _detail {

template <typename T>
using _wrapper_vector = std::vector<T>;

template <typename T>
using _wrapper_list = std::list<T>;

};

template <typename T>
using VectorView = ConstContainerView<_detail::_wrapper_vector, T>;

template <typename T>
using ListView = ConstContainerView<_detail::_wrapper_list, T>;

LCORE_NAMESPACE_END

