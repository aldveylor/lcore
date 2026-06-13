#pragma once
#include "base.hpp"
#include "container/view.hpp"
#include "enum.hpp"
#include <cstdint>

LCORE_NAMESPACE_BEGIN

enum class PropertyFlags : uint8_t {
    None = 0x00,
    Read = 0x01,
    Write = 0x02,
};
LCORE_ENUM_BITWISE_OPERATORS(LCORE_NAMESPACE::PropertyFlags)

template <typename Key>
class PropertyItemInfo {
public:
    Key key;
    TypeInfoPtr type;
    PropertyFlags flags;
};

template <typename Key, typename Ret = bool, typename KeyView = Key>
class PropertySet: public AbstractClass {
public:
    using ItemInfo = PropertyItemInfo<Key>;
    using ItemInfoView = Span<const ItemInfo>;

    virtual TypeInfoRef GetTypeInfo() const noexcept = 0;
    virtual ItemInfoView GetItems() const noexcept = 0;
    
    virtual Ret SetProperty(KeyView key, RawPtr<const void> value) = 0;
    virtual Ret GetProperty(KeyView key, RawPtr<void> value) const = 0;
};


LCORE_NAMESPACE_END

