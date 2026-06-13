#include <gtest/gtest.h>
#include <lcore/object/structmeta.hpp>
#include <lcore/object/enummeta.hpp>


using namespace LCORE_NAMESPACE;
using namespace LCORE_NAMESPACE::object;

// struct meta example

struct BaseType {};
using BaseTypeMeta = StructMetaBase<BaseType>;

struct MyStruct: BaseType {
    struct DataStruct {
        int a = 1;
        float b = 2.0f;
        unsigned char c = 3;
    };

    using Meta = BaseTypeMeta::MetaType<DataStruct>;
    static Meta thisMeta;
    DataStruct data;
};

MyStruct::Meta MyStruct::thisMeta = MyStruct::Meta::NewMeta<MyStruct::DataStruct>();
static auto _ = []() {
    return &MyStruct::thisMeta.AddKey<int>("a", offsetof(MyStruct::DataStruct, a))
        .AddKey<float>("b", offsetof(MyStruct::DataStruct, b))
        .AddKey("c", &MyStruct::DataStruct::c);
}();

struct MyComplexStruct: BaseType {
    struct DataStruct {
        MyStruct::DataStruct myStruct;
    };
    
    using Meta = BaseTypeMeta::MetaType<MyComplexStruct::DataStruct>;
    static Meta thisMeta;
    DataStruct data = {MyStruct::DataStruct()};
};

MyComplexStruct::Meta MyComplexStruct::thisMeta = MyComplexStruct::Meta::NewMeta<MyComplexStruct::DataStruct>();
static auto _complex = []() {
    return &MyComplexStruct::thisMeta.AddStructKey<MyStruct::DataStruct>("myStruct", offsetof(MyComplexStruct::DataStruct, myStruct));
}();

TEST(StructMetaTest, StructMetaCreation) {
    auto myStruct = MyStruct();
    auto& meta = MyStruct::thisMeta;
    EXPECT_EQ(meta.GetSize(), sizeof(MyStruct::DataStruct));
    EXPECT_EQ(meta.GetKeys().size(), 3);
    EXPECT_EQ(meta.GetKeyInfo("a")->offset, offsetof(MyStruct::DataStruct, a));
    EXPECT_EQ(meta.GetKeyInfo("b")->offset, offsetof(MyStruct::DataStruct, b));
    EXPECT_EQ(meta.GetKeyInfo("c")->offset, offsetof(MyStruct::DataStruct, c));

    // Try to read the values
    EXPECT_EQ(*reinterpret_cast<int*>(reinterpret_cast<char*>(&myStruct.data) + meta.GetKeyInfo("a")->offset), myStruct.data.a);
    EXPECT_EQ(*reinterpret_cast<float*>(reinterpret_cast<char*>(&myStruct.data) + meta.GetKeyInfo("b")->offset), myStruct.data.b);
    EXPECT_EQ(*reinterpret_cast<unsigned char*>(reinterpret_cast<char*>(&myStruct.data) + meta.GetKeyInfo("c")->offset), myStruct.data.c);
}

TEST(StructMetaTest, ComplexStructMetaCreation) {
    auto myComplexStruct = MyComplexStruct();
    auto& meta = MyComplexStruct::thisMeta;
    EXPECT_EQ(meta.GetSize(), sizeof(MyComplexStruct::DataStruct));
    EXPECT_EQ(meta.GetKeys().size(), 1);
    EXPECT_EQ(meta.GetKeyInfo("myStruct")->offset, offsetof(MyComplexStruct::DataStruct, myStruct));

    // Try to read the values
    auto& myStructMeta = MyStruct::thisMeta;
    auto myStructPtr = reinterpret_cast<MyStruct::DataStruct*>(reinterpret_cast<char*>(&myComplexStruct.data) + meta.GetKeyInfo("myStruct")->offset);
    EXPECT_EQ(myStructPtr->a, 1);
    EXPECT_EQ(myStructPtr->b, 2.0f);
    EXPECT_EQ(myStructPtr->c, 3);

    // Check if the struct meta can be found by type
    auto foundMeta = BaseTypeMeta::FindByType<MyComplexStruct::DataStruct>();
    EXPECT_EQ(foundMeta, &MyComplexStruct::thisMeta);
    EXPECT_EQ(foundMeta->GetSize(), sizeof(MyComplexStruct::DataStruct));
    EXPECT_EQ(foundMeta->GetKeyInfo("myStruct")->size, sizeof(MyStruct::DataStruct));
    EXPECT_EQ(foundMeta->GetKeyInfo("myStruct")->structMeta, &MyStruct::thisMeta);
}

// enum meta example

struct EnumBase {};
using EnumBaseMeta = EnumMetaBase<EnumBase>;

struct MyEnum: EnumBase {
    using Meta = EnumBaseMeta::MetaType<MyEnum>;
    static Meta thisMeta;

    using Enum = typename Meta::Enum;

    static Enum Value1;
    static Enum Value2;
    static Enum Value3;
};

MyEnum::Meta MyEnum::thisMeta = MyEnum::Meta::NewMeta<MyEnum>();
MyEnum::Enum MyEnum::Value1 = MyEnum::thisMeta.AddValue("Value1");
MyEnum::Enum MyEnum::Value2 = MyEnum::thisMeta.AddValue("Value2");
MyEnum::Enum MyEnum::Value3 = MyEnum::thisMeta.AddValue("Value3");

TEST(EnumMetaTest, EnumMetaCreation) {
    auto& meta = MyEnum::thisMeta;
    EXPECT_EQ(meta.GetValues().size(), 3);
    EXPECT_EQ(meta.GetValueInfo("Value1")->name, "Value1");
    EXPECT_EQ(meta.GetValueInfo("Value2")->name, "Value2");
    EXPECT_EQ(meta.GetValueInfo("Value3")->name, "Value3");

    // Check if the enum meta can be found by type
    auto foundMeta = EnumBaseMeta::FindByType<MyEnum>();
    EXPECT_EQ(foundMeta, &MyEnum::thisMeta);
    EXPECT_EQ(foundMeta->GetValues().size(), 3);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
