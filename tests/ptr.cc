#include <gtest/gtest.h>
#include "lcore/pointer.hpp"
#include <iostream>
#include <sstream>

using namespace LCORE_NAMESPACE_NAME;

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

TEST(PointerTest, SharedPtrRefTest) {
    std::stringstream ss;
    auto* originalCout = std::cout.rdbuf(ss.rdbuf()); // Redirect cout to stringstream
    
    struct TestClass {
        int value;
        TestClass(int v) : value(v) {}
        virtual ~TestClass() { std::cout << "TestClass destroyed with value: " << value << std::endl; }
    };

    struct DerivedClass : public TestClass {
        DerivedClass(int v) : TestClass(v) {}
        ~DerivedClass() { std::cout << "DerivedClass destroyed with value: " << value << std::endl; }
    };

    SharedPtr<TestClass> ptr1(new DerivedClass(42));
    EXPECT_TRUE(ptr1);
    EXPECT_EQ(ptr1->value, 42);

    {
        SharedPtr<TestClass> ptr2 = ptr1; // Copy constructor
        EXPECT_TRUE(ptr2);
        EXPECT_EQ(ptr2->value, 42);
    }
    // ptr2 goes out of scope, should not print anything yet

    {
        SharedPtr<TestClass> ptr3 = std::move(ptr1); // Move constructor
        EXPECT_TRUE(ptr3);
        EXPECT_EQ(ptr3->value, 42);
        EXPECT_FALSE(ptr1); // ptr1 should be null after move
    }
    // ptr3 goes out of scope, should print destruction messages

    std::cout.rdbuf(originalCout); // Restore cout
    std::string output = ss.str();
    EXPECT_TRUE(output.find("TestClass destroyed with value: 42") != std::string::npos);
    EXPECT_TRUE(output.find("DerivedClass destroyed with value: 42") != std::string::npos);
};

TEST(PointerTest, SharedPtrCastTest) {
    std::stringstream ss;
    auto* originalCout = std::cout.rdbuf(ss.rdbuf()); // Redirect cout to stringstream

    struct Base {
        int value = 1;
        virtual ~Base() { std::cout << "Base destroyed" << std::endl; }
    };

    struct Derived : public Base {
        int derivedValue = 2;
        ~Derived() { std::cout << "Derived destroyed" << std::endl; }
    };

    {
        SharedPtr<Base> basePtr(new Derived());
        EXPECT_TRUE(basePtr);
        EXPECT_EQ(basePtr->value, 1);
        
        SharedPtr<Derived> derivedPtr = basePtr.Cast<Derived>();
        EXPECT_TRUE(derivedPtr);
        EXPECT_EQ(derivedPtr->value, 1);
        EXPECT_EQ(derivedPtr->derivedValue, 2);

        basePtr.Reset(); // Reset the base pointer
        EXPECT_FALSE(basePtr);
        EXPECT_TRUE(derivedPtr); // Derived pointer should still be valid

        basePtr = derivedPtr;
        EXPECT_TRUE(basePtr);
        EXPECT_EQ(basePtr.UseCount(), 2);
    }

    std::cout.rdbuf(originalCout); // Restore cout
    std::string output = ss.str();
    EXPECT_TRUE(output.find("Base destroyed") != std::string::npos);
    EXPECT_TRUE(output.find("Derived destroyed") != std::string::npos);
}

TEST(PointerTest, SharedPtrNullTest) {
    SharedPtr<int> nullPtr;
    EXPECT_FALSE(nullPtr);
    EXPECT_EQ(nullPtr.Get(), nullptr);

    SharedPtr<int> initializedPtr(new int(10));
    EXPECT_TRUE(initializedPtr);
    EXPECT_EQ(*initializedPtr, 10);

    initializedPtr.Reset();
    EXPECT_FALSE(initializedPtr);
    EXPECT_EQ(initializedPtr.Get(), nullptr);
}

TEST(PointerTest, SharedPtrDeleterTest) {
    std::stringstream ss;
    auto* originalCout = std::cout.rdbuf(ss.rdbuf()); // Redirect cout to stringstream

    struct TestDeleter {
        void operator()(int* ptr) {
            std::cout << "Deleting int: " << *ptr << std::endl;
            delete ptr;
        }
    };

    SharedPtr<int> ptr(new int(42), TestDeleter());
    EXPECT_TRUE(ptr);
    EXPECT_EQ(*ptr, 42);

    ptr.Reset();
    EXPECT_FALSE(ptr);

    SharedPtr<int> ptr2(new int(43), [&ss](int* p) {
        ss << "Deleting int: " << *p << std::endl;
        delete p;
    });

    ptr2.Reset();
    EXPECT_FALSE(ptr2);

    std::cout.rdbuf(originalCout); // Restore cout
    std::string output = ss.str();
    EXPECT_TRUE(output.find("Deleting int: 42") != std::string::npos);
    EXPECT_TRUE(output.find("Deleting int: 43") != std::string::npos);
}

TEST(PointerTest, WeakPtrTest) {
    struct TestClass {
        int value;
        TestClass(int v) : value(v) {}
    };

    struct DerivedClass : public TestClass {
        DerivedClass(int v) : TestClass(v) {}
    };

    SharedPtr<TestClass> sharedPtr(new DerivedClass(42));
    EXPECT_TRUE(sharedPtr);
    EXPECT_EQ(sharedPtr->value, 42);

    WeakPtr<TestClass> weakPtr(sharedPtr);
    EXPECT_EQ(weakPtr.Lock()->value, 42);

    sharedPtr.Reset(); // Reset the shared pointer
    EXPECT_TRUE(weakPtr.Expired());
    EXPECT_FALSE(weakPtr.Lock());
    EXPECT_EQ(weakPtr.UseCount(), 0);
}

TEST(PointerTest, PtrComplexTest) {
    std::stringstream ss;
    auto* originalCout = std::cout.rdbuf(ss.rdbuf()); // Redirect cout to stringstream

    struct Handler {
    private:
        int value;
    public:
        Handler(int v) : value(v) {}
        int GetValue() const { return value; }
        void SetValue(int v) { value = v; }
        virtual ~Handler() = default;
    };

    struct DerivedHandler : public Handler {
        float extraValue;
    public:
        DerivedHandler(int v, float ev) : Handler(v), extraValue(ev) {}
        float GetFullValue() const { return GetValue() + extraValue; }
        void SetExtraValue(float ev) { SetValue(ev); extraValue = ev - GetValue(); }
        virtual ~DerivedHandler() {
            std::cout << "{value: " << GetValue() << ", extraValue: " << extraValue << "} DerivedHandler destroyed" << std::endl;
        }
    };

    struct Holder {
        SharedPtr<Handler> handlerPtr;
        Holder(SharedPtr<Handler> ptr) : handlerPtr(std::move(ptr)) {}
    };

    SharedPtr<Holder> holder(nullptr);
    {
        SharedPtr<DerivedHandler> handler(new DerivedHandler(10, 0.5f));
        holder = SharedPtr<Holder>(new Holder(handler));
    }
    EXPECT_TRUE(holder->handlerPtr);
    EXPECT_EQ(holder.UseCount(), 1);
    EXPECT_EQ(holder->handlerPtr.UseCount(), 1);
    EXPECT_EQ(holder->handlerPtr->GetValue(), 10);
    EXPECT_EQ(holder->handlerPtr.Get().Cast<DerivedHandler>()->GetFullValue(), 10.5f);

    SharedPtr<Holder> holder2 = holder; // Copy the holder
    EXPECT_EQ(holder.UseCount(), 2);
    EXPECT_EQ(holder2.UseCount(), 2);

    WeakPtr<Handler> weakHandler(holder->handlerPtr);

    EXPECT_FALSE(weakHandler.Expired());
    EXPECT_EQ(weakHandler.Lock()->GetValue(), 10);
    
    holder.Reset(); // Reset the shared pointer
    EXPECT_EQ(holder2.UseCount(), 1);

    EXPECT_FALSE(weakHandler.Expired());
    EXPECT_TRUE(weakHandler.Lock());

    holder2.Reset(); // Reset the holder
    EXPECT_FALSE(holder2);

    // Due to the reset, the DerivedHandler should be destroyed now
    EXPECT_TRUE(weakHandler.Expired());
    EXPECT_FALSE(weakHandler.Lock());

    std::cout.rdbuf(originalCout); // Restore cout
    std::string output = ss.str();
    EXPECT_TRUE(output.find("{value: 10, extraValue: 0.5} DerivedHandler destroyed") != std::string::npos);
}

TEST(PointerTest, UniquePtrTest) {
    std::stringstream ss;
    auto* originalCout = std::cout.rdbuf(ss.rdbuf()); // Redirect cout to stringstream

    struct TestClass {
        int value;
        TestClass(int v) : value(v) {}
        virtual ~TestClass() { std::cout << "TestClass destroyed with value: " << value << std::endl; }
    };

    UniquePtr<TestClass> uniquePtr(new TestClass(100));
    EXPECT_TRUE(uniquePtr);
    EXPECT_EQ(uniquePtr->value, 100);

    uniquePtr.Reset();
    EXPECT_FALSE(uniquePtr);

    std::cout.rdbuf(originalCout); // Restore cout
    std::string output = ss.str();
    EXPECT_TRUE(output.find("TestClass destroyed with value: 100") != std::string::npos);
}

TEST(PointerTest, SharedFromThis) {
    std::stringstream ss;
    auto* originalCout = std::cout.rdbuf(ss.rdbuf()); // Redirect cout to stringstream

    struct Base {
        virtual ~Base() = default;
    };

    struct EnableSharedFromThisTest : public EnableSharedFromThis<EnableSharedFromThisTest>, public Base {
        int value;
        EnableSharedFromThisTest(int v) : value(v) {}
        SharedPtr<EnableSharedFromThisTest> GetThis() { return SharedFromThis(); }

        virtual ~EnableSharedFromThisTest() { std::cout << "EnableSharedFromThisTest destroyed with value: " << value << std::endl; }
    };

    struct EnableSharedFromThisDerived : public EnableSharedFromThisTest {
        using EnableSharedFromThisTest::EnableSharedFromThisTest;
    };

    SharedPtr<EnableSharedFromThisTest> ptr(new EnableSharedFromThisTest(200));
    EXPECT_TRUE(ptr);
    EXPECT_EQ(ptr->value, 200);

    SharedPtr<EnableSharedFromThisTest> sharedPtr = ptr->GetThis();
    EXPECT_TRUE(sharedPtr);
    EXPECT_EQ(sharedPtr->value, 200);

    ptr.Reset(); // Reset the original pointer
    sharedPtr.Reset(); // Reset the shared pointer

    // SharedPtr<Base> basePtr(new EnableSharedFromThisDerived(300));
    auto basePtr = New<EnableSharedFromThisDerived>(300);
    EXPECT_TRUE(basePtr);
    EXPECT_EQ(basePtr.Cast<EnableSharedFromThisTest>()->value, 300);

    EXPECT_NO_THROW(basePtr.Cast<EnableSharedFromThisTest>()->GetThis());

    basePtr.Reset(); // Reset the base pointer
    EXPECT_FALSE(basePtr);

    std::cout.rdbuf(originalCout); // Restore cout
    std::string output = ss.str();
    EXPECT_TRUE(output.find("EnableSharedFromThisTest destroyed with value: 200") != std::string::npos);
    EXPECT_TRUE(output.find("EnableSharedFromThisTest destroyed with value: 300") != std::string::npos);
};
