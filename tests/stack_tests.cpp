/*
 *  <Short Description>
 *  Copyright (C) 2024  Brett Terpstra
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
#include <blt/gp/operations.h>
#include <blt/std/logging.h>
#include <blt/std/types.h>
#include <blt/std/random.h>
#include <random>
#include <iostream>

template<typename T, typename Func>
T make_data(T t, Func&& func)
{
    for (const auto& [index, v] : blt::enumerate(t.data))
        v = func(index);
    return t;
}

template<typename T, typename U>
blt::ptrdiff_t compare(const T& t, const U& u)
{
    for (const auto& [index, v] : blt::enumerate(t.data))
    {
        if (u.data[index] != v)
            return static_cast<blt::ptrdiff_t>(index);
    }
    return -1;
}

#define MAKE_VARIABLE(SIZE) large_##SIZE base_##SIZE = make_data(large_##SIZE{}, [](auto index) { \
   return static_cast<blt::u8>(blt::random::murmur_random64c<blt::size_t>(SEED + index, 0, 256)); \
});                                                                                               \
large_##SIZE secondary_##SIZE = make_data(large_##SIZE{}, [](auto index) {                        \
   return static_cast<blt::u8>(blt::random::murmur_random64c<blt::size_t>(SEED + index, 0, 256)); \
});                                                                                               \
large_##SIZE tertiary_##SIZE = make_data(large_##SIZE{}, [](auto index) {                         \
   return static_cast<blt::u8>(blt::random::murmur_random64c<blt::size_t>(SEED + index, 0, 256)); \
})

#define RUN_TEST(FAILURE_COND, STACK, PASS, ...) do { if (FAILURE_COND) { BLT_ERROR(__VA_ARGS__); } else { BLT_DEBUG_STREAM << PASS << " | " << STACK.size() << "\n"; } } while(false)
#define RUN_TEST_SIZE(VALUE, STACK) RUN_TEST(auto index = compare(VALUE, STACK.pop<decltype(VALUE)>()); index >= 0, STACK, blt::type_string<decltype(VALUE)>() + " test PASSED.", "Failed to pop large value (" + blt::type_string<decltype(VALUE)>() + "), failed at index %ld", index)
#define RUN_TEST_TYPE(EXPECTED, STACK) RUN_TEST(auto val = STACK.pop<decltype(EXPECTED)>(); val != EXPECTED, STACK, blt::type_string<decltype(EXPECTED)>() + " test PASSED", "Failed to pop correct " + blt::type_string<decltype(EXPECTED)>() + " (" #EXPECTED ") found %lf", val);

const blt::u64 SEED = std::random_device()();

struct large_256
{
    blt::u8 data[256];
};

struct large_2048
{
    blt::u8 data[2048];
};

struct large_4096
{
    blt::u8 data[4096];
};

struct large_6123
{
    blt::u8 data[6123];
};

struct large_18290
{
    blt::u8 data[18290];
};

MAKE_VARIABLE(256);
MAKE_VARIABLE(2048);
MAKE_VARIABLE(4096);
MAKE_VARIABLE(6123);
MAKE_VARIABLE(18290);

void test_basic_types()
{
    BLT_INFO("Testing pushing types, will transfer and pop off each stack.");
    blt::gp::stack_allocator stack;
    stack.push(50.0f);
    BLT_TRACE_STREAM << "Pushed float: " << stack.size() << "\n";
    stack.push(base_2048);
    BLT_TRACE_STREAM << "Pushed 2048: " << stack.size() << "\n";
    stack.push(25.0f);
    BLT_TRACE_STREAM << "Pushed float: " << stack.size() << "\n";
    stack.push(-24.0f);
    BLT_TRACE_STREAM << "Pushed float: " << stack.size() << "\n";
    stack.push(base_256);
    BLT_TRACE_STREAM << "Pushed 256: " << stack.size() << "\n";
    stack.push(secondary_256);
    BLT_TRACE_STREAM << "Pushed 256*: " << stack.size() << "\n";
    stack.push(false);
    BLT_TRACE_STREAM << "Pushed bool: " << stack.size() << "\n";
    stack.push(523);
    BLT_TRACE_STREAM << "Pushed int: " << stack.size() << "\n";
    stack.push(base_6123);
    BLT_TRACE_STREAM << "Pushed 6123: " << stack.size() << "\n";
    
    std::cout << std::endl;
    
    {
        BLT_INFO("Popping 6123, int, and bool via transfer");
        blt::gp::stack_allocator to;
        stack.transfer_bytes(to, sizeof(large_6123));
        stack.transfer_bytes(to, sizeof(int));
        stack.transfer_bytes(to, sizeof(bool));
        RUN_TEST_TYPE(false, to);
        RUN_TEST_TYPE(523, to);
        RUN_TEST_SIZE(base_6123, to);
        
        BLT_ASSERT(to.empty() && "Stack isn't empty despite all values popped!");
    }
    
    BLT_TRACE_STREAM << stack.size() << "\n";
    std::cout << std::endl;
    
    BLT_INFO("Pushing new data onto partially removed stack, this will test re-allocating blocks. We will also push at least one more block.");
    stack.push(tertiary_256);
    BLT_TRACE_STREAM << "Pushed 256^: " << stack.size() << "\n";
    stack.push(69.999);
    BLT_TRACE_STREAM << "Pushed double: " << stack.size() << "\n";
    stack.push(secondary_2048);
    BLT_TRACE_STREAM << "Pushed 2048*: " << stack.size() << "\n";
    stack.push(420.6900001);
    BLT_TRACE_STREAM << "Pushed double: " << stack.size() << "\n";
    stack.push(base_256);
    BLT_TRACE_STREAM << "Pushed 256: " << stack.size() << "\n";
    stack.push(base_18290);
    BLT_TRACE_STREAM << "Pushed 18290: " << stack.size() << "\n";
    std::cout << std::endl;
    
    {
        BLT_INFO("Popping all data via transfer.");
        blt::gp::stack_allocator to;
        stack.transfer_bytes(to, sizeof(large_18290));
        stack.transfer_bytes(to, sizeof(large_256));
        stack.transfer_bytes(to, sizeof(double));
        stack.transfer_bytes(to, sizeof(large_2048));
        stack.transfer_bytes(to, sizeof(double));
        stack.transfer_bytes(to, sizeof(large_256));
        
        RUN_TEST_SIZE(tertiary_256, to);
        RUN_TEST_TYPE(69.999, to);
        RUN_TEST_SIZE(secondary_2048, to);
        RUN_TEST_TYPE(420.6900001, to);
        RUN_TEST_SIZE(base_256, to);
        RUN_TEST_SIZE(base_18290, to);
        
        BLT_ASSERT(to.empty() && "Stack isn't empty despite all values popped!");
    }
    
    BLT_TRACE_STREAM << stack.size() << "\n";
    std::cout << std::endl;
    
    BLT_INFO("Now we will test using large values where the unallocated blocks do not have enough storage.");
    stack.push(secondary_18290);
    BLT_TRACE_STREAM << "Pushed 18290*: " << stack.size() << "\n";
    stack.push(base_4096);
    BLT_TRACE_STREAM << "Pushed 4096: " << stack.size() << "\n";
    stack.push(tertiary_18290);
    BLT_TRACE_STREAM << "Pushed 18290^: " << stack.size() << "\n";
    stack.push(secondary_6123);
    BLT_TRACE_STREAM << "Pushed 6123*: " << stack.size() << "\n";
    std::cout << std::endl;
    
    {
        BLT_INFO("Popping values normally.");
        RUN_TEST_SIZE(secondary_6123, stack);
        RUN_TEST_SIZE(tertiary_18290, stack);
        RUN_TEST_SIZE(base_4096, stack);
        RUN_TEST_SIZE(secondary_18290, stack);
    }
    BLT_TRACE_STREAM << stack.size() << "\n";
    std::cout << std::endl;
    
    BLT_INFO("Some fishy numbers in the last reported size. Let's try modifying the stack."); // fixed by moving back in pop
    stack.push(88.9f);
    BLT_TRACE_STREAM << "Pushed float: " << stack.size() << "\n";
    
    {
        BLT_INFO("Popping a few values.");
        RUN_TEST_TYPE(88.9f, stack);
        RUN_TEST_SIZE(secondary_256, stack);
    }
    BLT_TRACE_STREAM << stack.size() << "\n";
    std::cout << std::endl;
    
    BLT_INFO("We will now empty the stack and try to reuse it.");
    {
        RUN_TEST_SIZE(base_256, stack);
        RUN_TEST_TYPE(-24.0f, stack);
        RUN_TEST_TYPE(25.0f, stack);
        RUN_TEST_SIZE(base_2048, stack);
        RUN_TEST_TYPE(50.0f, stack);
    }
    BLT_TRACE_STREAM << stack.size() << "\n";
    std::cout << std::endl;
}

int main()
{
    test_basic_types();
}