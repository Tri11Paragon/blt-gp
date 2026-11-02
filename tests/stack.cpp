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
#include <cstdlib>
#include <functional>
#include <utility>
#include <type_traits>

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

    // Compile-time trait validations
    static_assert(std::is_trivially_copyable_v<Pod1>, "Pod1 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod2>, "Pod2 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod3>, "Pod3 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod5>, "Pod5 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod7>, "Pod7 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod12>, "Pod12 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod16>, "Pod16 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod24>, "Pod24 should be trivially copyable");
    static_assert(std::is_trivially_copyable_v<Pod31>, "Pod31 should be trivially copyable");

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
             .push(Pod16{9876, 55})                              // +16 bytes (aligned)
             .push(3.14159)                                      // +8 bytes (aligned)
             .push(uint32_t{9999})                               // +8 bytes (aligned)
             .push(Pod7{{1, 2, 3, 4, 5, 6, 7}});                 // +8 bytes (aligned)

        size_t expected_total = 8 + 16 + detail::aligned_size<Pod16> + 8 + 8 + 8;
        TEST_ASSERT(stack.size() == expected_total);

        // Pop in reverse order
        Pod7 p7 = stack.pop<Pod7>();
        TEST_ASSERT(p7 == (Pod7{{1, 2, 3, 4, 5, 6, 7}}));

        uint32_t value = stack.pop<uint32_t>();
        TEST_ASSERT(value == 9999);

        double d = stack.pop<double>();
        TEST_ASSERT(d == 3.14159);

        Pod16 p16 = stack.pop<Pod16>();
        TEST_ASSERT(p16 == (Pod16{9876, 55}));

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

    void test_stack_replace_erase_clear()
    {
        std::cout << "[TEST] test_stack_replace_erase_clear..." << std::endl;

        // Replacing one 8-byte slot with a 16-byte value grows the destination.
        byte_stack_t growing_stack;
        growing_stack.push(int32_t{10});
        growing_stack.push(uint32_t{20});
        growing_stack.push(uint16_t{30});

        byte_stack_t larger_value;
        larger_value.push(Pod12{1, 2, 3});

        auto& growing_ref = growing_stack.replace(larger_value, 8, 16);
        TEST_ASSERT(&growing_ref == &growing_stack);
        TEST_ASSERT(growing_stack.size() == 32);
        TEST_ASSERT(larger_value.size() == 16);
        TEST_ASSERT(growing_stack.pop<uint16_t>() == 30);
        TEST_ASSERT(growing_stack.pop<Pod12>() == (Pod12{1, 2, 3}));
        TEST_ASSERT(growing_stack.pop<int32_t>() == 10);
        TEST_ASSERT(growing_stack.empty());

        // Replacing a 16-byte value with an 8-byte value shrinks the destination.
        byte_stack_t shrinking_stack;
        shrinking_stack.push(int32_t{40});
        shrinking_stack.push(Pod12{4, 5, 6});
        shrinking_stack.push(uint32_t{70});

        byte_stack_t smaller_value;
        smaller_value.push(uint64_t{50});

        shrinking_stack.replace(smaller_value, 8, 24);
        TEST_ASSERT(shrinking_stack.size() == 24);
        TEST_ASSERT(shrinking_stack.pop<uint32_t>() == 70);
        TEST_ASSERT(shrinking_stack.pop<uint64_t>() == 50);
        TEST_ASSERT(shrinking_stack.pop<int32_t>() == 40);
        TEST_ASSERT(shrinking_stack.empty());

        // An empty replaced range inserts the replacement between intact values.
        byte_stack_t inserted_stack;
        inserted_stack.push(int32_t{1});
        inserted_stack.push(uint64_t{3});
        inserted_stack.replace(larger_value, 8, 8);
        TEST_ASSERT(inserted_stack.size() == 32);
        TEST_ASSERT(inserted_stack.pop<uint64_t>() == 3);
        TEST_ASSERT(inserted_stack.pop<Pod12>() == (Pod12{1, 2, 3}));
        TEST_ASSERT(inserted_stack.pop<int32_t>() == 1);

        // Erasing a complete middle slot preserves the values around it.
        byte_stack_t erased_stack;
        erased_stack.push(int32_t{11});
        erased_stack.push(Pod12{12, 13, 14});
        erased_stack.push(uint64_t{15});
        erased_stack.erase(8, 24);
        TEST_ASSERT(erased_stack.size() == 16);
        TEST_ASSERT(erased_stack.pop<uint64_t>() == 15);
        TEST_ASSERT(erased_stack.pop<int32_t>() == 11);
        TEST_ASSERT(erased_stack.empty());

        // Erasing an empty range is a no-op.
        erased_stack.push(Pod12{21, 22, 23});
        erased_stack.erase(8, 8);
        TEST_ASSERT(erased_stack.size() == 16);
        TEST_ASSERT(erased_stack.pop<Pod12>() == (Pod12{21, 22, 23}));

        // clear() releases the current logical contents and the stack remains reusable.
        erased_stack.push(uint64_t{99});
        erased_stack.clear();
        TEST_ASSERT(erased_stack.empty());
        TEST_ASSERT(erased_stack.size() == 0);
        erased_stack.push(Pod12{31, 32, 33});
        const auto& const_stack = erased_stack;
        TEST_ASSERT(const_stack.top<Pod12>() == (Pod12{31, 32, 33}));
        TEST_ASSERT(erased_stack.pop<Pod12>() == (Pod12{31, 32, 33}));
        TEST_ASSERT(erased_stack.empty());
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

    void test_top_with_offset()
    {
        std::cout << "[TEST] test_top_with_offset..." << std::endl;

        byte_stack_t stack;

        // Push 3 distinct arguments: arg1 (int32_t), arg2 (double), arg3 (Pod12)
        // Stack layout (bottom to top):
        // [0..7]   : int32_t(42)              -> aligned size 8
        // [8..15]  : double(3.14159)          -> aligned size 8
        // [16..31] : Pod12{10, 20, 30}        -> aligned size 16
        // Total size: 32 bytes
        stack.push(int32_t{42});
        stack.push(double{3.14159});
        stack.push(Pod12{10, 20, 30});

        TEST_ASSERT(stack.size() == 32);

        // 1. Verify default offset (0) accesses the top element
        TEST_ASSERT(stack.top<Pod12>() == (Pod12{10, 20, 30}));
        TEST_ASSERT(stack.top<Pod12>(0) == (Pod12{10, 20, 30}));

        // 2. Extract arguments using offsets relative to top
        // Offset for top element (Pod12) = 0
        const Pod12& arg3 = stack.top<Pod12>(0);
        TEST_ASSERT(arg3 == (Pod12{10, 20, 30}));

        // Offset for second element (double) = aligned_size<Pod12> = 16
        const size_t offset_arg2 = detail::aligned_size<Pod12>;
        const double& arg2 = stack.top<double>(offset_arg2);
        TEST_ASSERT(arg2 == 3.14159);

        // Offset for third element (int32_t) = aligned_size<Pod12> + aligned_size<double> = 16 + 8 = 24
        const size_t offset_arg1 = offset_arg2 + detail::aligned_size<double>;
        const int32_t& arg1 = stack.top<int32_t>(offset_arg1);
        TEST_ASSERT(arg1 == 42);

        // 3. Simulate function argument evaluation and invocation
        auto eval_fn = [](int32_t a, double b, const Pod12& c) -> double {
            return static_cast<double>(a) + b + static_cast<double>(c.a + c.b + c.c);
        };
        double result = eval_fn(arg1, arg2, arg3);
        double expected_result = 42.0 + 3.14159 + (10.0 + 20.0 + 30.0);
        TEST_ASSERT(result == expected_result);

        // 4. Verify in-place mutation via returned reference at offset
        stack.top<int32_t>(offset_arg1) = 100;
        stack.top<double>(offset_arg2) = 2.71828;
        stack.top<Pod12>(0).b = 555;

        TEST_ASSERT(stack.top<int32_t>(offset_arg1) == 100);
        TEST_ASSERT(stack.top<double>(offset_arg2) == 2.71828);
        TEST_ASSERT(stack.top<Pod12>(0) == (Pod12{10, 555, 30}));

        // 5. Remove all extracted arguments at once via pop_bytes
        size_t total_args_bytes = detail::aligned_size<Pod12> + detail::aligned_size<double> + detail::aligned_size<int32_t>;
        size_t popped = stack.pop_bytes(total_args_bytes);
        TEST_ASSERT(popped == 32);
        TEST_ASSERT(stack.size() == 0);

        // 6. Test deeper stack with multiple nested frame parameters
        // Frame 0: [uint64_t(111)] (8 bytes)
        // Frame 1: [uint8_t(2), Pod16{33, 44}, float(5.5f)] (8 + 16 + 8 = 32 bytes)
        stack.push(uint64_t{111});
        stack.push(uint8_t{2});
        stack.push(Pod16{33, 44});
        stack.push(float{5.5f});

        TEST_ASSERT(stack.size() == 40);

        // Extract Frame 1 parameters
        float param_float = stack.top<float>(0);
        TEST_ASSERT(param_float == 5.5f);

        Pod16 param_pod16 = stack.top<Pod16>(detail::aligned_size<float>);
        TEST_ASSERT(param_pod16 == (Pod16{33, 44}));

        uint8_t param_u8 = stack.top<uint8_t>(detail::aligned_size<float> + detail::aligned_size<Pod16>);
        TEST_ASSERT(param_u8 == 2);

        // Pop Frame 1 (32 bytes)
        stack.pop_bytes(detail::aligned_size<float> + detail::aligned_size<Pod16> + detail::aligned_size<uint8_t>);
        TEST_ASSERT(stack.size() == 8);

        // Verify Frame 0 is still intact and accessible
        TEST_ASSERT(stack.top<uint64_t>() == 111);
        TEST_ASSERT(stack.pop<uint64_t>() == 111);
        TEST_ASSERT(stack.size() == 0);
    }

    int free_fn_sub(int a, int b)
    {
        return a - b;
    }

    struct CustomCallableStruct
    {
        int multiplier;
        int operator()(int a, int b) const
        {
            return (a + b) * multiplier;
        }
    };

    void test_call_with_arguments()
    {
        std::cout << "[TEST] test_call_with_arguments..." << std::endl;

        // 1. Validate argument order (LIFO stack layout: first arg is deepest, last arg is top)
        {
            byte_stack_t stack;
            // Function: (a - b) / c
            auto div_sub = [](int a, int b, int c) -> int {
                return (a - b) / c;
            };

            // Push args in sequential order: a = 100, b = 20, c = 4
            // Stack layout (bottom to top): [100 (a), 20 (b), 4 (c)]
            // c is at top of stack (offset 0), b is at offset 8, a is at offset 16
            stack.push(100);
            stack.push(20);
            stack.push(4);

            TEST_ASSERT(stack.size() == 24);

            int result = stack.call_with_arguments(div_sub);
            TEST_ASSERT(result == (100 - 20) / 4); // (100 - 20) / 4 = 20

            // Verify call does not remove elements from stack
            TEST_ASSERT(stack.size() == 24);
            TEST_ASSERT(stack.top<int>(0) == 4);
            TEST_ASSERT(stack.top<int>(8) == 20);
            TEST_ASSERT(stack.top<int>(16) == 100);
        }

        // 2. Heterogeneous argument types of varied sizes and alignments
        {
            byte_stack_t stack;
            // Arguments:
            // Arg1: uint8_t (aligned 8)
            // Arg2: Pod12 (aligned 16)
            // Arg3: double (aligned 8)
            // Arg4: Pod16 (aligned 16)
            auto complex_fn = [](uint8_t a, const Pod12& b, double c, const Pod16& d) -> double {
                return static_cast<double>(a)
                     + static_cast<double>(b.a + b.b + b.c)
                     + c
                     + static_cast<double>(d.a + d.b);
            };

            stack.push(static_cast<uint8_t>(5));
            stack.push(Pod12{10, 20, 30});
            stack.push(0.5);
            stack.push(Pod16{100, 200});

            size_t expected_size = 8 + 16 + 8 + 16;
            TEST_ASSERT(stack.size() == expected_size);

            double res = stack.call_with_arguments(complex_fn);
            double expected_val = 5.0 + (10.0 + 20.0 + 30.0) + 0.5 + (100.0 + 200.0); // 365.5
            TEST_ASSERT(res == expected_val);

            // Stack contents unchanged
            TEST_ASSERT(stack.size() == expected_size);
        }

        // 3. Multiple chained function calls where 1st result is pushed and used by 2nd function
        {
            byte_stack_t stack;

            // Pipeline:
            // Step 1: add(a, b) -> sum
            stack.push(25);
            stack.push(15);
            TEST_ASSERT(stack.size() == 16);

            auto f1 = [](int a, int b) { return a + b; };
            int sum = stack.call_with_arguments(f1);
            TEST_ASSERT(sum == 40);

            // Pop args used for 1st function call
            stack.pop_bytes(detail::aligned_size<int> * 2);
            TEST_ASSERT(stack.size() == 0);

            // Step 2: Push newly computed sum and second argument 'multiplier'
            stack.push(sum); // 40
            stack.push(3);   // factor
            TEST_ASSERT(stack.size() == 16);

            auto f2 = [](int val, int factor) { return val * factor; };
            int product = stack.call_with_arguments(f2);
            TEST_ASSERT(product == 120);

            // Pop args used for 2nd function call
            stack.pop_bytes(detail::aligned_size<int> * 2);
            TEST_ASSERT(stack.size() == 0);

            // Step 3: Push product and floating point offset
            stack.push(product); // 120
            stack.push(0.75);    // offset
            TEST_ASSERT(stack.size() == 16);

            auto f1_1 = [](int val, double off) { return static_cast<double>(val) + off; };
            double final_result = stack.call_with_arguments(f1_1);
            TEST_ASSERT(final_result == 120.75);

            // Pop args used for 3rd function call and push result
            stack.pop_bytes(detail::aligned_size<int> + detail::aligned_size<double>);
            TEST_ASSERT(stack.size() == 0);

            stack.push(final_result);
            TEST_ASSERT(stack.size() == 8);
            TEST_ASSERT(stack.pop<double>() == 120.75);
            TEST_ASSERT(stack.size() == 0);
        }

        // 4. Nested sub-expression tree evaluation with an existing base frame
        {
            byte_stack_t stack;

            // Base frame (e.g. context ID)
            stack.push(uint64_t{99999});

            // Sub-expression 1: a + b (where a=10, b=20)
            stack.push(10);
            stack.push(20);
            auto f3 = [](int a, int b) { return a + b; };
            int sub1 = stack.call_with_arguments(f3);
            TEST_ASSERT(sub1 == 30);
            stack.pop_bytes(detail::aligned_size<int> * 2);
            stack.push(sub1); // push 30 on top of base frame

            // Sub-expression 2: c * d (where c=4, d=5)
            stack.push(4);
            stack.push(5);
            auto f4 = [](int c, int d) { return c * d; };
            int sub2 = stack.call_with_arguments(f4);
            TEST_ASSERT(sub2 == 20);
            stack.pop_bytes(detail::aligned_size<int> * 2);
            stack.push(sub2); // push 20 on top of [base frame, sub1]

            // Stack layout now: [uint64_t(99999), int(30), int(20)]
            TEST_ASSERT(stack.size() == 8 + 8 + 8);

            // Evaluate top-level function combining base context and sub-expression results
            auto f5 = [](uint64_t base, int r1, int r2) {
                return base + static_cast<uint64_t>(r1 * r2);
            };
            uint64_t total = stack.call_with_arguments(f5);
            TEST_ASSERT(total == 99999 + (30 * 20)); // 99999 + 600 = 100599

            // Verify stack frame integrity
            TEST_ASSERT(stack.pop<int>() == 20);
            TEST_ASSERT(stack.pop<int>() == 30);
            TEST_ASSERT(stack.pop<uint64_t>() == 99999);
            TEST_ASSERT(stack.size() == 0);
        }

        // 5. Various callable forms (free functions, lambdas with captures, std::function, custom functor)
        {
            byte_stack_t stack;
            stack.push(50);
            stack.push(18);

            // Free function pointer
            int r1 = stack.call_with_arguments(free_fn_sub);
            TEST_ASSERT(r1 == 32);

            // Stateful lambda with capture
            int capture_val = 1000;
            auto stateful_lambda = [&capture_val](int a, int b) {
                return a + b + capture_val;
            };
            int r2 = stack.call_with_arguments(stateful_lambda);
            TEST_ASSERT(r2 == 50 + 18 + 1000);

            // std::function
            std::function<int(int, int)> std_fn = [](int a, int b) { return a * b; };
            int r3 = stack.call_with_arguments(std_fn);
            TEST_ASSERT(r3 == 50 * 18);

            // Custom functor object
            CustomCallableStruct custom_obj{10};
            int r4 = stack.call_with_arguments(custom_obj);
            TEST_ASSERT(r4 == (50 + 18) * 10);

            // Noexcept lambda
            auto noexcept_fn = [](int a, int b) noexcept -> int { return a ^ b; };
            int r5 = stack.call_with_arguments(noexcept_fn);
            TEST_ASSERT(r5 == (50 ^ 18));

            stack.pop_bytes(16);
            TEST_ASSERT(stack.size() == 0);
        }

        // 6. In-place argument mutation via non-const references
        {
            byte_stack_t stack;
            stack.push(Pod12{1, 2, 3});
            stack.push(100);

            auto f6 = [](Pod12& p, int& x) {
                p.a += 10;
                p.b += 20;
                p.c += 30;
                x *= 2;
            };
            // Function taking non-const reference to mutate arguments in-place on stack
            stack.call_with_arguments(f6);

            TEST_ASSERT(stack.top<int>() == 200);
            TEST_ASSERT(stack.top<Pod12>(detail::aligned_size<int>) == (Pod12{11, 22, 33}));

            stack.pop_bytes(detail::aligned_size<int> + detail::aligned_size<Pod12>);
            TEST_ASSERT(stack.size() == 0);
        }

        // 7. Const stack support
        {
            byte_stack_t stack;
            stack.push(Pod12{7, 8, 9});
            stack.push(42);

            const byte_stack_t<>& const_ref = stack;
            auto f7 = [](const Pod12& p, int x) {
                return p.a + p.b + p.c + x;
            };
            int sum = const_ref.call_with_arguments(f7);
            TEST_ASSERT(sum == 7 + 8 + 9 + 42);

            stack.pop_bytes(detail::aligned_size<int> + detail::aligned_size<Pod12>);
            TEST_ASSERT(stack.size() == 0);
        }

        // 8. Zero-argument function (arity = 0)
        {
            byte_stack_t stack;
            stack.push(12345); // existing data on stack

            auto f8 = []() {
                return 777;
            };
            int zero_res = stack.call_with_arguments(f8);
            TEST_ASSERT(zero_res == 777);
            TEST_ASSERT(stack.size() == 8);
            TEST_ASSERT(stack.pop<int>() == 12345);
            TEST_ASSERT(stack.size() == 0);
        }

        // 9. Single-argument function (arity = 1)
        {
            byte_stack_t stack;
            stack.push(Pod12{100, 200, 300});

            auto f9 = [](const Pod12& p) {
                return p.a + p.b + p.c;
            };
            int sum = stack.call_with_arguments(f9);
            TEST_ASSERT(sum == 600);
            TEST_ASSERT(stack.pop<Pod12>() == (Pod12{100, 200, 300}));
            TEST_ASSERT(stack.size() == 0);
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
    test_pop_bytes();
    test_heterogeneous_complex_sequence();
    test_stack_insert_merging();
    test_stack_replace_erase_clear();
    test_reallocation_and_stress();
    test_stack_move_semantics();
    test_copy_from();
    test_copy_range();
    test_copy_back();
    test_copy_back_offset();
    test_top_with_offset();
    test_call_with_arguments();

    std::cout << "========================================" << std::endl;
    std::cout << " All byte_stack_t tests PASSED!         " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}