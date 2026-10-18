/*
 *  Copyright (C) 2026  Brett Terpstra
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <blt/gp/stack.h>
#include <iostream>
#include <cstdint>
#include <string>
#include <memory>
#include <vector>
#include <type_traits>
#include <cassert>

#include "blt/logging/logging.h"

#define TEST_ASSERT(cond)                                                                      \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "FAILED assertion: " #cond " at " << __FILE__ << ":" << __LINE__ << std::endl; \
            std::exit(1);                                                                       \
        }                                                                                       \
    } while (false)

#define TEST_ASSERT_MSG(cond, msg)                                                              \
    do {                                                                                        \
        if (!(cond)) {                                                                          \
            std::cerr << "FAILED assertion: " #cond " (" << (msg) << ") at "                   \
                      << __FILE__ << ":" << __LINE__ << std::endl;                             \
            std::exit(1);                                                                       \
        }                                                                                       \
    } while (false)

namespace
{
    using namespace blt::gp;

    // ------------------------------------------------------------------------
    // Test Type Definitions
    // ------------------------------------------------------------------------

    // Trivially copyable PODs of varied sizes
    struct alignas(1) Pod1
    {
        uint8_t a;
        bool operator==(const Pod1& o) const { return a == o.a; }
    };

    struct alignas(1) Pod2
    {
        uint8_t a;
        uint8_t b;
        bool operator==(const Pod2& o) const { return a == o.a && b == o.b; }
    };

    struct alignas(1) Pod3
    {
        uint8_t a, b, c;
        bool operator==(const Pod3& o) const { return a == o.a && b == o.b && c == o.c; }
    };

    struct alignas(1) Pod5
    {
        uint8_t data[5];
        bool operator==(const Pod5& o) const
        {
            for (int i = 0; i < 5; ++i)
                if (data[i] != o.data[i]) return false;
            return true;
        }
    };

    struct alignas(1) Pod7
    {
        uint8_t data[7];
        bool operator==(const Pod7& o) const
        {
            for (int i = 0; i < 7; ++i)
                if (data[i] != o.data[i]) return false;
            return true;
        }
    };

    struct Pod12
    {
        int32_t a, b, c;
        bool operator==(const Pod12& o) const { return a == o.a && b == o.b && c == o.c; }
    };

    struct Pod16
    {
        uint64_t a, b;
        bool operator==(const Pod16& o) const { return a == o.a && b == o.b; }
    };

    struct Pod24
    {
        uint64_t a, b, c;
        bool operator==(const Pod24& o) const { return a == o.a && b == o.b && c == o.c; }
    };

    struct alignas(1) Pod31
    {
        uint8_t data[31];
        bool operator==(const Pod31& o) const
        {
            for (int i = 0; i < 31; ++i)
                if (data[i] != o.data[i]) return false;
            return true;
        }
    };

    // Copy-only type (copy constructible, NOT move constructible)
    struct CopyOnlyType
    {
        int value;
        static inline int copy_construct_count = 0;
        static inline int copy_assign_count = 0;
        static inline int destructor_count = 0;

        explicit CopyOnlyType(int v = 0) : value(v) {}
        CopyOnlyType(const CopyOnlyType& o) : value(o.value) { ++copy_construct_count; }
        CopyOnlyType(CopyOnlyType&&) = delete;
        CopyOnlyType& operator=(const CopyOnlyType& o) { value = o.value; ++copy_assign_count; return *this; }
        CopyOnlyType& operator=(CopyOnlyType&&) = delete;
        ~CopyOnlyType() { ++destructor_count; }

        static void reset_counters()
        {
            copy_construct_count = 0;
            copy_assign_count = 0;
            destructor_count = 0;
        }
    };

    // Move-only type (move constructible, NOT copy constructible)
    struct MoveOnlyType
    {
        int value;
        static inline int move_construct_count = 0;
        static inline int move_assign_count = 0;
        static inline int destructor_count = 0;

        explicit MoveOnlyType(int v = 0) : value(v) {}
        MoveOnlyType(const MoveOnlyType&) = delete;
        MoveOnlyType(MoveOnlyType&& o) noexcept : value(o.value) { o.value = -1; ++move_construct_count; }
        MoveOnlyType& operator=(const MoveOnlyType&) = delete;
        MoveOnlyType& operator=(MoveOnlyType&& o) noexcept { value = o.value; o.value = -1; ++move_assign_count; return *this; }
        ~MoveOnlyType() { ++destructor_count; }

        static void reset_counters()
        {
            move_construct_count = 0;
            move_assign_count = 0;
            destructor_count = 0;
        }
    };

    // Type with both copy and move constructors
    struct CopyAndMoveType
    {
        int* dynamic_val;
        int value;
        static inline int copy_construct_count = 0;
        static inline int move_construct_count = 0;
        static inline int destructor_count = 0;

        explicit CopyAndMoveType(int dyn = 0, int v = 0)
            : dynamic_val(new int(dyn)), value(v) {}

        CopyAndMoveType(const CopyAndMoveType& o)
            : dynamic_val(o.dynamic_val ? new int(*o.dynamic_val) : nullptr), value(o.value)
        {
            ++copy_construct_count;
        }

        CopyAndMoveType(CopyAndMoveType&& o) noexcept
            : dynamic_val(o.dynamic_val), value(o.value)
        {
            o.dynamic_val = nullptr;
            o.value = -1;
            ++move_construct_count;
        }

        CopyAndMoveType& operator=(const CopyAndMoveType& o)
        {
            if (this != &o)
            {
                delete dynamic_val;
                dynamic_val = o.dynamic_val ? new int(*o.dynamic_val) : nullptr;
                value = o.value;
            }
            return *this;
        }

        CopyAndMoveType& operator=(CopyAndMoveType&& o) noexcept
        {
            if (this != &o)
            {
                delete dynamic_val;
                dynamic_val = o.dynamic_val;
                value = o.value;
                o.dynamic_val = nullptr;
                o.value = -1;
            }
            return *this;
        }

        ~CopyAndMoveType()
        {
            delete dynamic_val;
            ++destructor_count;
        }

        static void reset_counters()
        {
            copy_construct_count = 0;
            move_construct_count = 0;
            destructor_count = 0;
        }
    };

    // Immobile type (neither copyable nor movable)
    struct ImmobileType
    {
        int a;
        double b;
        explicit ImmobileType(int a = 0, double b = 0.0) : a(a), b(b) {}
        ImmobileType(const ImmobileType&) = delete;
        ImmobileType(ImmobileType&&) = delete;
        ImmobileType& operator=(const ImmobileType&) = delete;
        ImmobileType& operator=(ImmobileType&&) = delete;
    };

    // Compile-time trait validations
    static_assert(std::is_trivially_copyable_v<Pod1>, "Pod1 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod3>, "Pod3 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod5>, "Pod5 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod7>, "Pod7 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod12>, "Pod12 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod16>, "Pod16 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod24>, "Pod24 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod31>, "Pod31 should be trivially copyable");

    static_assert(!std::is_trivially_copyable_v<CopyOnlyType>, "CopyOnlyType is not trivially copyable");
    static_assert(std::is_copy_constructible_v<CopyOnlyType>, "CopyOnlyType should be copy constructible");
    static_assert(!std::is_move_constructible_v<CopyOnlyType>, "CopyOnlyType should not be move constructible");

    static_assert(!std::is_trivially_copyable_v<MoveOnlyType>, "MoveOnlyType is not trivially copyable");
    static_assert(!std::is_copy_constructible_v<MoveOnlyType>, "MoveOnlyType should not be copy constructible");
    static_assert(std::is_move_constructible_v<MoveOnlyType>, "MoveOnlyType should be move constructible");

    static_assert(std::is_copy_constructible_v<CopyAndMoveType>, "CopyAndMoveType should be copy constructible");
    static_assert(std::is_move_constructible_v<CopyAndMoveType>, "CopyAndMoveType should be move constructible");

    static_assert(!std::is_copy_constructible_v<ImmobileType>, "ImmobileType should not be copy constructible");
    static_assert(!std::is_move_constructible_v<ImmobileType>, "ImmobileType should not be move constructible");

    // ------------------------------------------------------------------------
    // Tests
    // ------------------------------------------------------------------------

    void test_constructors_and_capacity()
    {
        std::cout << "[TEST] test_constructors_and_capacity..." << std::endl;

        // Default constructor
        byte_stack_t stack;
        TEST_ASSERT(stack.size() == 0);

        // Reserve
        stack.reserve(128);
        TEST_ASSERT(stack.size() == 0);
        TEST_ASSERT(stack.capacity() >= 128);

        // Sized constructor
        byte_stack_t sized_stack(64);
        TEST_ASSERT(sized_stack.size() == 0);
        TEST_ASSERT(sized_stack.capacity() >= 64);

        // Data alignment (must be 8-byte aligned)
        if (stack.capacity() > 0)
        {
            TEST_ASSERT(reinterpret_cast<uintptr_t>(stack.data()) % 8 == 0);
        }
        if (sized_stack.capacity() > 0)
        {
            const auto& const_ref = sized_stack;
            TEST_ASSERT(reinterpret_cast<uintptr_t>(sized_stack.data()) % 8 == 0);
            TEST_ASSERT(reinterpret_cast<uintptr_t>(const_ref.data()) % 8 == 0);
        }
    }

    void test_alignment_and_allocation_sizes()
    {
        std::cout << "[TEST] test_alignment_and_allocation_sizes..." << std::endl;

        byte_stack_t stack;
        size_t expected_size = 0;

        auto test_push_type = [&](auto&& val, size_t expected_aligned_delta) {
            using T = std::decay_t<decltype(val)>;
            size_t size_before = stack.size();
            stack.push(std::forward<decltype(val)>(val));
            size_t size_after = stack.size();

            expected_size += expected_aligned_delta;
            TEST_ASSERT_MSG(size_after == expected_size, "Stack size mismatch after push");
            TEST_ASSERT_MSG(size_after - size_before == expected_aligned_delta, "Delta size mismatch");

            // Verify top pointer alignment (must be exactly 8-byte aligned)
            T& top_ref = stack.top<T>();
            uintptr_t addr = reinterpret_cast<uintptr_t>(&top_ref);
            TEST_ASSERT_MSG(addr % 8 == 0, "Top element address is not 8-byte aligned");
        };

        // sizeof(Pod1) = 1 -> aligned to 8
        test_push_type(Pod1{42}, 8);
        // sizeof(Pod2) = 2 -> aligned to 8
        test_push_type(Pod2{1, 2}, 8);
        // sizeof(Pod3) = 3 -> aligned to 8
        test_push_type(Pod3{10, 20, 30}, 8);
        // sizeof(int32_t) = 4 -> aligned to 8
        test_push_type(int32_t{123456}, 8);
        // sizeof(Pod5) = 5 -> aligned to 8
        test_push_type(Pod5{{1, 2, 3, 4, 5}}, 8);
        // sizeof(Pod7) = 7 -> aligned to 8
        test_push_type(Pod7{{7, 6, 5, 4, 3, 2, 1}}, 8);
        // sizeof(uint64_t) = 8 -> aligned to 8
        test_push_type(uint64_t{0xDEADBEEFCAFEBABEllu}, 8);
        // sizeof(Pod12) = 12 -> aligned to 16
        test_push_type(Pod12{100, 200, 300}, 16);
        // sizeof(Pod16) = 16 -> aligned to 16
        test_push_type(Pod16{0x1111222233334444llu, 0x5555666677778888llu}, 16);
        // sizeof(Pod24) = 24 -> aligned to 24
        test_push_type(Pod24{1, 2, 3}, 24);
        // sizeof(Pod31) = 31 -> aligned to 32
        Pod31 p31{};
        for (uint8_t i = 0; i < 31; ++i) p31.data[i] = i;
        test_push_type(p31, 32);

        // Pop all and verify LIFO order and sizes
        TEST_ASSERT(stack.pop<Pod31>() == p31);
        expected_size -= 32;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod24>() == (Pod24{1, 2, 3}));
        expected_size -= 24;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod16>() == (Pod16{0x1111222233334444llu, 0x5555666677778888llu}));
        expected_size -= 16;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod12>() == (Pod12{100, 200, 300}));
        expected_size -= 16;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<uint64_t>() == 0xDEADBEEFCAFEBABEllu);
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod7>() == (Pod7{{7, 6, 5, 4, 3, 2, 1}}));
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod5>() == (Pod5{{1, 2, 3, 4, 5}}));
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<int32_t>() == 123456);
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod3>() == (Pod3{10, 20, 30}));
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod2>() == (Pod2{1, 2}));
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);

        TEST_ASSERT(stack.pop<Pod1>() == (Pod1{42}));
        expected_size -= 8;
        TEST_ASSERT(stack.size() == expected_size);
        TEST_ASSERT(stack.size() == 0);
    }

    void test_trivially_copyable_types()
    {
        std::cout << "[TEST] test_trivially_copyable_types..." << std::endl;

        byte_stack_t stack;

        // Push primitives and PODs
        stack.push(static_cast<uint8_t>(255));
        stack.push(static_cast<int16_t>(-1234));
        stack.push(3.141592653589793);
        stack.push(Pod12{10, 20, 30});

        // Test top() read & write in-place
        TEST_ASSERT(stack.top<Pod12>() == (Pod12{10, 20, 30}));
        stack.top<Pod12>().b = 999;
        TEST_ASSERT(stack.top<Pod12>() == (Pod12{10, 999, 30}));

        // Pop in reverse order
        Pod12 p = stack.pop<Pod12>();
        TEST_ASSERT(p == (Pod12{10, 999, 30}));

        double d = stack.pop<double>();
        TEST_ASSERT(d == 3.141592653589793);

        int16_t s = stack.pop<int16_t>();
        TEST_ASSERT(s == -1234);

        uint8_t u = stack.pop<uint8_t>();
        TEST_ASSERT(u == 255);

        TEST_ASSERT(stack.size() == 0);
    }

    void test_copy_only_types()
    {
        std::cout << "[TEST] test_copy_only_types..." << std::endl;

        CopyOnlyType::reset_counters();
        {
            byte_stack_t stack;
            CopyOnlyType obj1{111};
            CopyOnlyType obj2{222};

            // Push lvalues (invoke copy ctor during in-place construction)
            stack.push(obj1);
            TEST_ASSERT(CopyOnlyType::copy_construct_count == 1);
            TEST_ASSERT(stack.size() == detail::aligned_size<CopyOnlyType>);

            stack.push(obj2);
            TEST_ASSERT(CopyOnlyType::copy_construct_count == 2);
            TEST_ASSERT(stack.size() == detail::aligned_size<CopyOnlyType> * 2);

            // Test top
            TEST_ASSERT(stack.top<CopyOnlyType>().value == 222);

            // Test pop (must copy-construct the returned value since move is deleted)
            int copies_before = CopyOnlyType::copy_construct_count;
            CopyOnlyType popped2 = stack.pop<CopyOnlyType>();
            TEST_ASSERT(popped2.value == 222);
            TEST_ASSERT(CopyOnlyType::copy_construct_count >= copies_before + 1);
            TEST_ASSERT(stack.size() == detail::aligned_size<CopyOnlyType>);

            TEST_ASSERT(stack.top<CopyOnlyType>().value == 111);
            CopyOnlyType popped1 = stack.pop<CopyOnlyType>();
            TEST_ASSERT(popped1.value == 111);
            TEST_ASSERT(stack.size() == 0);
        }
    }

    void test_move_only_types()
    {
        std::cout << "[TEST] test_move_only_types..." << std::endl;

        MoveOnlyType::reset_counters();
        {
            byte_stack_t stack;
            MoveOnlyType m1{555};
            MoveOnlyType m2{777};

            stack.push(std::move(m1));
            TEST_ASSERT(m1.value == -1); // source was moved from
            TEST_ASSERT(MoveOnlyType::move_construct_count == 1);

            stack.push(std::move(m2));
            TEST_ASSERT(m2.value == -1);
            TEST_ASSERT(MoveOnlyType::move_construct_count == 2);

            TEST_ASSERT(stack.top<MoveOnlyType>().value == 777);

            // Test pop (moves out of top into returned value)
            int moves_before = MoveOnlyType::move_construct_count;
            MoveOnlyType popped2 = stack.pop<MoveOnlyType>();
            TEST_ASSERT(popped2.value == 777);
            TEST_ASSERT(MoveOnlyType::move_construct_count == moves_before + 1);

            MoveOnlyType popped1 = stack.pop<MoveOnlyType>();
            TEST_ASSERT(popped1.value == 555);
            TEST_ASSERT(stack.size() == 0);
        }

        // Test with std::unique_ptr (standard move-only type)
        {
            byte_stack_t stack;
            auto ptr1 = std::make_unique<int>(42);
            auto ptr2 = std::make_unique<int>(84);

            stack.push(std::move(ptr1));
            TEST_ASSERT(ptr1 == nullptr);

            stack.push(std::move(ptr2));
            TEST_ASSERT(ptr2 == nullptr);

            TEST_ASSERT(*stack.top<std::unique_ptr<int>>() == 84);

            std::unique_ptr<int> popped2 = stack.pop<std::unique_ptr<int>>();
            TEST_ASSERT(popped2 != nullptr && *popped2 == 84);

            std::unique_ptr<int> popped1 = stack.pop<std::unique_ptr<int>>();
            TEST_ASSERT(popped1 != nullptr && *popped1 == 42);

            TEST_ASSERT(stack.size() == 0);
        }
    }

    void test_copy_and_move_types()
    {
        std::cout << "[TEST] test_copy_and_move_types..." << std::endl;

        CopyAndMoveType::reset_counters();
        {
            byte_stack_t stack;

            // Push rvalue -> should move
            stack.push(CopyAndMoveType{123, 10});
            TEST_ASSERT(CopyAndMoveType::move_construct_count >= 1);

            // Push lvalue -> should copy
            CopyAndMoveType item2{456, 20};
            int copies_before = CopyAndMoveType::copy_construct_count;
            stack.push(item2);
            TEST_ASSERT(CopyAndMoveType::copy_construct_count == copies_before + 1);

            // Inspect top
            TEST_ASSERT(stack.top<CopyAndMoveType>().dynamic_val != nullptr && *stack.top<CopyAndMoveType>().dynamic_val == 456);
            TEST_ASSERT(stack.top<CopyAndMoveType>().value == 20);

            // pop() on a type with both copy and move must use MOVE constructor
            int moves_before = CopyAndMoveType::move_construct_count;
            copies_before = CopyAndMoveType::copy_construct_count;

            CopyAndMoveType popped2 = stack.pop<CopyAndMoveType>();
            TEST_ASSERT(popped2.dynamic_val != nullptr && *popped2.dynamic_val == 456);
            TEST_ASSERT(popped2.value == 20);
            TEST_ASSERT(CopyAndMoveType::move_construct_count == moves_before + 1);
            TEST_ASSERT(CopyAndMoveType::copy_construct_count == copies_before);

            CopyAndMoveType popped1 = stack.pop<CopyAndMoveType>();
            TEST_ASSERT(popped1.dynamic_val != nullptr && *popped1.dynamic_val == 123);
            TEST_ASSERT(popped1.value == 10);
            TEST_ASSERT(stack.size() == 0);
        }
    }

    void test_pop_bytes()
    {
        std::cout << "[TEST] test_pop_bytes..." << std::endl;

        byte_stack_t stack;

        // Push Pod12 and Pod7
        stack.push(Pod12{100, 200, 300});
        stack.push(Pod7{{1, 2, 3, 4, 5, 6, 7}});
        TEST_ASSERT(stack.size() == 16 + 8);

        // Read and modify via top()
        TEST_ASSERT(stack.top<Pod7>() == (Pod7{{1, 2, 3, 4, 5, 6, 7}}));
        stack.top<Pod7>().data[0] = 99;
        TEST_ASSERT(stack.top<Pod7>().data[0] == 99);

        // Pop using pop_bytes<T>()
        size_t popped_bytes = stack.pop_bytes<Pod7>();
        TEST_ASSERT(popped_bytes == 8);
        TEST_ASSERT(stack.size() == 16);
        TEST_ASSERT(stack.top<Pod12>() == (Pod12{100, 200, 300}));

        popped_bytes = stack.pop_bytes<Pod12>();
        TEST_ASSERT(popped_bytes == 16);
        TEST_ASSERT(stack.size() == 0);

        // Test pop_bytes(size_t) with various raw byte counts
        stack.push(uint64_t{1});
        stack.push(uint64_t{2});
        stack.push(uint64_t{3});
        TEST_ASSERT(stack.size() == 24);

        // Pop 1 raw byte -> should align to 8 and pop 8 bytes
        size_t pb = stack.pop_bytes(1);
        TEST_ASSERT(pb == 8);
        TEST_ASSERT(stack.size() == 16);
        TEST_ASSERT(stack.top<uint64_t>() == 2);

        // Pop 10 raw bytes -> should align to 16 and pop 16 bytes
        pb = stack.pop_bytes(10);
        TEST_ASSERT(pb == 16);
        TEST_ASSERT(stack.size() == 0);
    }

    void test_heterogeneous_complex_sequence()
    {
        std::cout << "[TEST] test_heterogeneous_complex_sequence..." << std::endl;

        byte_stack_t stack;

        // Push a sequence of heterogeneous types
        stack.push(static_cast<uint8_t>(42))                     // +8 bytes (aligned)
             .push(Pod12{1, 2, 3})                               // +16 bytes (aligned)
             .push(CopyAndMoveType{9876, 55})                    // +16 bytes (aligned)
             .push(3.14159)                                      // +8 bytes (aligned)
             .push(std::make_unique<int>(9999))                  // +8 bytes (aligned)
             .push(Pod7{{1, 2, 3, 4, 5, 6, 7}});                 // +8 bytes (aligned)

        size_t expected_total = 8 + 16 + detail::aligned_size<CopyAndMoveType> + 8 + 8 + 8;
        TEST_ASSERT(stack.size() == expected_total);

        // Pop in reverse order
        Pod7 p7 = stack.pop<Pod7>();
        TEST_ASSERT(p7 == (Pod7{{1, 2, 3, 4, 5, 6, 7}}));

        std::unique_ptr<int> uptr = stack.pop<std::unique_ptr<int>>();
        TEST_ASSERT(uptr != nullptr && *uptr == 9999);

        double d = stack.pop<double>();
        TEST_ASSERT(d == 3.14159);

        CopyAndMoveType cm = stack.pop<CopyAndMoveType>();
        TEST_ASSERT(cm.dynamic_val != nullptr && *cm.dynamic_val == 9876 && cm.value == 55);

        Pod12 p12 = stack.pop<Pod12>();
        TEST_ASSERT(p12 == (Pod12{1, 2, 3}));

        uint8_t u8 = stack.pop<uint8_t>();
        TEST_ASSERT(u8 == 42);

        TEST_ASSERT(stack.size() == 0);
    }

    void test_stack_insert_merging()
    {
        std::cout << "[TEST] test_stack_insert_merging..." << std::endl;

        byte_stack_t stack1;
        stack1.push(int32_t{10});
        stack1.push(int32_t{20});

        byte_stack_t stack2;
        stack2.push(Pod12{100, 200, 300});
        stack2.push(double{99.9});

        size_t size1_orig = stack1.size();
        size_t size2_orig = stack2.size();

        // Insert stack2 into stack1
        stack1.insert(stack2);

        // stack2 should now be cleared
        TEST_ASSERT(stack2.size() == 0);
        // stack1 size should be sum of both
        TEST_ASSERT(stack1.size() == size1_orig + size2_orig);

        // Pop all items in LIFO order from stack1
        double d = stack1.pop<double>();
        TEST_ASSERT(d == 99.9);

        Pod12 p = stack1.pop<Pod12>();
        TEST_ASSERT(p == (Pod12{100, 200, 300}));

        int32_t v2 = stack1.pop<int32_t>();
        TEST_ASSERT(v2 == 20);

        int32_t v1 = stack1.pop<int32_t>();
        TEST_ASSERT(v1 == 10);

        TEST_ASSERT(stack1.size() == 0);

        // Test inserting empty stack
        byte_stack_t empty_stack;
        stack1.insert(empty_stack);
        TEST_ASSERT(stack1.size() == 0);
        TEST_ASSERT(empty_stack.size() == 0);
    }

    void test_reallocation_and_stress()
    {
        std::cout << "[TEST] test_reallocation_and_stress..." << std::endl;

        byte_stack_t stack;
        constexpr int NUM_ITEMS = 500;

        // Push 500 sets of varied elements to trigger multiple vector reallocations
        for (int i = 0; i < NUM_ITEMS; ++i)
        {
            stack.push(static_cast<uint8_t>(i % 256));
            stack.push(Pod12{i, i * 2, i * 3});
            stack.push(static_cast<double>(i * 1.5));
        }

        size_t expected_size_per_iter = 8 + 16 + 8; // 32 bytes
        TEST_ASSERT(stack.size() == NUM_ITEMS * expected_size_per_iter);

        // Pop in reverse order and verify data integrity
        for (int i = NUM_ITEMS - 1; i >= 0; --i)
        {
            double d = stack.pop<double>();
            TEST_ASSERT(d == static_cast<double>(i * 1.5));

            Pod12 p = stack.pop<Pod12>();
            TEST_ASSERT(p == (Pod12{i, i * 2, i * 3}));

            uint8_t u = stack.pop<uint8_t>();
            TEST_ASSERT(u == static_cast<uint8_t>(i % 256));
        }

        TEST_ASSERT(stack.size() == 0);
    }

    void test_stack_move_semantics()
    {
        std::cout << "[TEST] test_stack_move_semantics..." << std::endl;

        byte_stack_t stack1;
        stack1.push(42);
        stack1.push(Pod16{0xAA, 0xBB});
        stack1.push(Pod12{7, 8, 9});

        size_t orig_size = stack1.size();

        // Move constructor
        byte_stack_t stack2 = std::move(stack1);
        TEST_ASSERT(stack2.size() == orig_size);

        // Move assignment
        byte_stack_t stack3;
        stack3 = std::move(stack2);
        TEST_ASSERT(stack3.size() == orig_size);

        // Pop from stack3 and verify integrity
        Pod12 p = stack3.pop<Pod12>();
        TEST_ASSERT(p == (Pod12{7, 8, 9}));

        Pod16 p16 = stack3.pop<Pod16>();
        TEST_ASSERT(p16 == (Pod16{0xAA, 0xBB}));

        int v = stack3.pop<int>();
        TEST_ASSERT(v == 42);

        TEST_ASSERT(stack3.size() == 0);
    }

    void test_copy_from()
    {
        std::cout << "[TEST] test_copy_from..." << std::endl;

        byte_stack_t src;
        src.push(int32_t{10});            // +8 bytes (index 0..7)
        src.push(double{20.5});           // +8 bytes (index 8..15)
        src.push(Pod12{100, 200, 300});   // +16 bytes (index 16..31)
        src.push(uint64_t{99999});        // +8 bytes (index 32..39)

        const size_t orig_src_size = src.size();
        TEST_ASSERT(orig_src_size == 40);

        // 1. Copy everything from index 0
        {
            byte_stack_t dst;
            auto& ref = dst.copy(src, 0);
            TEST_ASSERT(&ref == &dst); // Verify chaining reference
            TEST_ASSERT(dst.size() == orig_src_size);
            TEST_ASSERT(src.size() == orig_src_size); // Source must not be modified or cleared

            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.pop<double>() == 20.5);
            TEST_ASSERT(dst.pop<int32_t>() == 10);
            TEST_ASSERT(dst.size() == 0);
        }

        // 2. Copy partial from index 16 (skipping first two elements: int32_t and double)
        {
            byte_stack_t dst;
            dst.push(static_cast<uint8_t>(1)); // Pre-existing element in destination
            dst.copy(src, 16);

            TEST_ASSERT(dst.size() == 8 + (orig_src_size - 16));
            TEST_ASSERT(src.size() == orig_src_size);

            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.pop<uint8_t>() == 1);
            TEST_ASSERT(dst.size() == 0);
        }

        // 3. Copy with from == src.size() (empty slice copied)
        {
            byte_stack_t dst;
            dst.push(42);
            dst.copy(src, src.size());
            TEST_ASSERT(dst.size() == 8);
            TEST_ASSERT(dst.pop<int>() == 42);
        }
    }

    void test_copy_range()
    {
        std::cout << "[TEST] test_copy_range..." << std::endl;

        byte_stack_t src;
        src.push(int32_t{10});            // bytes [0, 8)
        src.push(double{20.5});           // bytes [8, 16)
        src.push(Pod12{100, 200, 300});   // bytes [16, 32)
        src.push(uint64_t{99999});        // bytes [32, 40)

        const size_t orig_src_size = src.size();
        TEST_ASSERT(orig_src_size == 40);

        // 1. Copy middle range [8, 32) -> double and Pod12
        {
            byte_stack_t dst;
            auto& ref = dst.copy(src, 8, 32);
            TEST_ASSERT(&ref == &dst);
            TEST_ASSERT(dst.size() == 24);
            TEST_ASSERT(src.size() == orig_src_size);

            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.pop<double>() == 20.5);
            TEST_ASSERT(dst.size() == 0);
        }

        // 2. Copy single element range [0, 8) -> int32_t
        {
            byte_stack_t dst;
            dst.copy(src, 0, 8);
            TEST_ASSERT(dst.size() == 8);
            TEST_ASSERT(dst.pop<int32_t>() == 10);
            TEST_ASSERT(dst.size() == 0);
        }

        // 3. Copy entire range [0, 40)
        {
            byte_stack_t dst;
            dst.copy(src, 0, orig_src_size);
            TEST_ASSERT(dst.size() == orig_src_size);

            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.pop<double>() == 20.5);
            TEST_ASSERT(dst.pop<int32_t>() == 10);
            TEST_ASSERT(dst.size() == 0);
        }

        // 4. Copy empty range [16, 16)
        {
            byte_stack_t dst;
            dst.copy(src, 16, 16);
            TEST_ASSERT(dst.size() == 0);
        }

        // 5. Chained range copy
        {
            byte_stack_t dst;
            dst.copy(src, 0, 8)
               .copy(src, 32, 40);
            TEST_ASSERT(dst.size() == 16);
            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.pop<int32_t>() == 10);
            TEST_ASSERT(dst.size() == 0);
        }
    }

    void test_copy_back()
    {
        std::cout << "[TEST] test_copy_back..." << std::endl;

        byte_stack_t src;
        src.push(int32_t{10});            // [0..7]
        src.push(double{20.5});           // [8..15]
        src.push(Pod12{100, 200, 300});   // [16..31]
        src.push(uint64_t{99999});        // [32..39]

        const size_t orig_src_size = src.size();
        TEST_ASSERT(orig_src_size == 40);

        // 1. Copy back 8 bytes (top element: uint64_t)
        {
            byte_stack_t dst;
            auto& ref = dst.copy_back(src, 8);
            TEST_ASSERT(&ref == &dst);
            TEST_ASSERT(dst.size() == 8);
            TEST_ASSERT(src.size() == orig_src_size);

            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.size() == 0);
        }

        // 2. Copy back 24 bytes (top two elements: Pod12 + uint64_t)
        {
            byte_stack_t dst;
            dst.copy_back(src, 24);
            TEST_ASSERT(dst.size() == 24);

            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.size() == 0);
        }

        // 3. Copy back all 40 bytes
        {
            byte_stack_t dst;
            dst.copy_back(src, 40);
            TEST_ASSERT(dst.size() == 40);

            TEST_ASSERT(dst.pop<uint64_t>() == 99999);
            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.pop<double>() == 20.5);
            TEST_ASSERT(dst.pop<int32_t>() == 10);
            TEST_ASSERT(dst.size() == 0);
        }

        // 4. Copy back 0 bytes
        {
            byte_stack_t dst;
            dst.copy_back(src, 0);
            TEST_ASSERT(dst.size() == 0);
        }
    }

    void test_copy_back_offset()
    {
        std::cout << "[TEST] test_copy_back_offset..." << std::endl;

        byte_stack_t src;
        src.push(int32_t{10});            // [0..7]
        src.push(double{20.5});           // [8..15]
        src.push(Pod12{100, 200, 300});   // [16..31]
        src.push(uint64_t{99999});        // [32..39]

        const size_t orig_src_size = src.size();
        TEST_ASSERT(orig_src_size == 40);

        // 1. Copy Pod12 (16 bytes) with offset of 8 bytes from top (skipping uint64_t)
        // Range: [40 - 16 - 8, 40 - 8) = [16, 32)
        {
            byte_stack_t dst;
            auto& ref = dst.copy_back(src, 16, 8);
            TEST_ASSERT(&ref == &dst);
            TEST_ASSERT(dst.size() == 16);
            TEST_ASSERT(src.size() == orig_src_size);

            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.size() == 0);
        }

        // 2. Copy double and Pod12 (24 bytes) with offset of 8 bytes (skipping uint64_t)
        // Range: [40 - 24 - 8, 40 - 8) = [8, 32)
        {
            byte_stack_t dst;
            dst.copy_back(src, 24, 8);
            TEST_ASSERT(dst.size() == 24);

            TEST_ASSERT(dst.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(dst.pop<double>() == 20.5);
            TEST_ASSERT(dst.size() == 0);
        }

        // 3. Copy int32_t (8 bytes) at bottom with offset 32 (skipping top 32 bytes)
        // Range: [40 - 8 - 32, 40 - 32) = [0, 8)
        {
            byte_stack_t dst;
            dst.copy_back(src, 8, 32);
            TEST_ASSERT(dst.size() == 8);

            TEST_ASSERT(dst.pop<int32_t>() == 10);
            TEST_ASSERT(dst.size() == 0);
        }

        // 4. Offset = 0 should behave identically to copy_back(src, bytes)
        {
            byte_stack_t dst;
            dst.copy_back(src, 16, 0);
            TEST_ASSERT(dst.size() == 16);

            // Top 16 bytes is uint64_t (8 bytes) + second half of Pod12 (8 bytes)
            // But let's verify with an aligned 16 byte struct:
            byte_stack_t src2;
            src2.push(Pod16{111, 222});
            src2.push(Pod16{333, 444});

            byte_stack_t dst2;
            dst2.copy_back(src2, 16, 0);
            TEST_ASSERT(dst2.size() == 16);
            TEST_ASSERT(dst2.pop<Pod16>() == (Pod16{333, 444}));
        }

        // 5. Bytes = 0 should copy nothing
        {
            byte_stack_t dst;
            dst.copy_back(src, 0, 8);
            TEST_ASSERT(dst.size() == 0);
        }
    }
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " Running byte_stack_t Unit Test Suite   " << std::endl;
    std::cout << "========================================" << std::endl;

    test_constructors_and_capacity();
    test_alignment_and_allocation_sizes();
    test_trivially_copyable_types();
    test_copy_only_types();
    test_move_only_types();
    test_copy_and_move_types();
    test_pop_bytes();
    test_heterogeneous_complex_sequence();
    test_stack_insert_merging();
    test_reallocation_and_stress();
    test_stack_move_semantics();
    test_copy_from();
    test_copy_range();
    test_copy_back();
    test_copy_back_offset();

    std::cout << "========================================" << std::endl;
    std::cout << " All byte_stack_t tests PASSED!         " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}