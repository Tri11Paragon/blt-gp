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

struct log_box
{
    public:
        log_box(const std::string& text, blt::logging::logger logger): text(text), logger(logger)
        {
            logger << text << '\n';
        }
        
        ~log_box()
        {
            for ([[maybe_unused]] auto& _ : text)
                logger << '-';
            logger << '\n';
        }
    
    private:
        std::string text;
        blt::logging::logger logger;
};

template<typename T, typename Func>
T make_data(T t, Func&& func)
{
    for (const auto& [index, v] : blt::enumerate(t.data))
        v = func(index);
    return t;
}

template<typename T, typename Class, blt::size_t size>
static inline auto constexpr array_size(const T(Class::*)[size])
{
    return size;
}

template<typename T, typename U>
blt::ptrdiff_t compare(const T& t, const U& u)
{
    constexpr auto ts = array_size(&T::data);
    constexpr auto us = array_size(&U::data);
    BLT_ASSERT_MSG(ts == us, ("Array sizes don't match! " + std::to_string(ts) + " vs " + std::to_string(us)).c_str());
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

// not actually 4096 but will fill the whole page (4096)
struct large_4096
{
    blt::u8 data[blt::gp::stack_allocator::page_size_no_block()];
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
    log_box box("-----------------------{Stack Testing}-----------------------", BLT_INFO_STREAM);
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
    
    BLT_NEWLINE();
    
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
    BLT_NEWLINE();
    
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
    BLT_NEWLINE();
    
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
    BLT_NEWLINE();
    
    BLT_INFO("Now we will test using large values where the unallocated blocks do not have enough storage.");
    stack.push(secondary_18290);
    BLT_TRACE_STREAM << "Pushed 18290*: " << stack.size() << "\n";
    stack.push(base_4096);
    BLT_TRACE_STREAM << "Pushed 4096: " << stack.size() << "\n";
    stack.push(tertiary_18290);
    BLT_TRACE_STREAM << "Pushed 18290^: " << stack.size() << "\n";
    stack.push(secondary_6123);
    BLT_TRACE_STREAM << "Pushed 6123*: " << stack.size() << "\n";
    BLT_NEWLINE();
    
    {
        BLT_INFO("Popping values normally.");
        RUN_TEST_SIZE(secondary_6123, stack);
        RUN_TEST_SIZE(tertiary_18290, stack);
        RUN_TEST_SIZE(base_4096, stack);
        RUN_TEST_SIZE(secondary_18290, stack);
    }
    BLT_TRACE_STREAM << stack.size() << "\n";
    BLT_NEWLINE();
    
    BLT_INFO("Some fishy numbers in the last reported size. Let's try modifying the stack."); // fixed by moving back in pop
    stack.push(88.9f);
    BLT_TRACE_STREAM << "Pushed float: " << stack.size() << "\n";
    
    {
        BLT_INFO("Popping a few values.");
        RUN_TEST_TYPE(88.9f, stack);
        RUN_TEST_SIZE(secondary_256, stack);
    }
    BLT_TRACE_STREAM << stack.size() << "\n";
    BLT_NEWLINE();
    
    BLT_INFO("We will now empty the stack and try to reuse it.");
    {
        RUN_TEST_SIZE(base_256, stack);
        RUN_TEST_TYPE(-24.0f, stack);
        RUN_TEST_TYPE(25.0f, stack);
        RUN_TEST_SIZE(base_2048, stack);
        RUN_TEST_TYPE(50.0f, stack);
    }
    BLT_TRACE_STREAM << stack.size() << "\n";
    BLT_NEWLINE();
    
    stack.push(tertiary_18290);
    BLT_TRACE_STREAM << "Pushed 18290^: " << stack.size() << "\n";
    stack.push(base_4096);
    BLT_TRACE_STREAM << "Pushed 4096: " << stack.size() << "\n";
    stack.push(50);
    BLT_TRACE_STREAM << "Pushed int: " << stack.size() << "\n";
    BLT_NEWLINE();
    
    BLT_INFO("Clearing stack one final time");
    RUN_TEST_TYPE(50, stack);
    RUN_TEST_SIZE(base_4096, stack);
    RUN_TEST_SIZE(tertiary_18290, stack);
    BLT_TRACE_STREAM << stack.size() << "\n";
}

blt::gp::operation_t basic_2([](float a, float b) {
    BLT_ASSERT(a == 50.0f);
    BLT_ASSERT(b == 10.0f);
    return a + b;
});

blt::gp::operation_t basic_mixed_4([](float a, float b, bool i, bool p) {
    BLT_ASSERT(a == 50.0f);
    BLT_ASSERT(b == 10.0f);
    BLT_ASSERT(i);
    BLT_ASSERT(!p);
    return (a * (i ? 1.0f : 0.0f)) + (b * (p ? 1.0f : 0.0f));
});

blt::gp::operation_t large_256_basic_3([](const large_256& l, float a, float b) {
    BLT_ASSERT(compare(l, base_256) == -1);
    BLT_ASSERT_MSG(a == 691, std::to_string(a).c_str());
    BLT_ASSERT_MSG(b == 69.420f, std::to_string(b).c_str());
    return blt::black_box_ret(l);
});

blt::gp::operation_t large_4096_basic_3b([](const large_4096& l, float a, bool b) {
    BLT_ASSERT(compare(l, base_4096) == -1);
    BLT_ASSERT(a == 33);
    BLT_ASSERT(b);
    return blt::black_box_ret(l);
});

blt::gp::operation_t large_18290_basic_3b([](const large_18290& l, float a, bool b) {
    BLT_ASSERT(compare(l, base_18290) == -1);
    BLT_ASSERT(a == -2543);
    BLT_ASSERT(b);
    return blt::black_box_ret(l);
});

void test_basic()
{
    BLT_INFO("Testing basic with stack");
    {
        blt::gp::stack_allocator stack;
        stack.push(50.0f);
        stack.push(10.0f);
        BLT_TRACE_STREAM << stack.size() << "\n";
        basic_2.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<float>();
        RUN_TEST(val != 60.000000f, stack, "Basic 2 Test Passed", "Basic 2 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after basic evaluation.");
    }
    BLT_INFO("Testing basic with stack over boundary");
    {
        blt::gp::stack_allocator stack;
        stack.push(std::array<blt::u8, blt::gp::stack_allocator::page_size_no_block() - sizeof(float)>{});
        stack.push(50.0f);
        stack.push(10.0f);
        auto size = stack.size();
        BLT_TRACE_STREAM << size << "\n";
        BLT_ASSERT(size.blocks > 1 && "Stack doesn't have more than one block!");
        basic_2.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<float>();
        stack.pop<std::array<blt::u8, blt::gp::stack_allocator::page_size_no_block() - sizeof(float)>>();
        RUN_TEST(val != 60.000000f, stack, "Basic 2 Boundary Test Passed", "Basic 2 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after basic evaluation over stack boundary");
    }
}

void test_mixed()
{
    BLT_INFO("Testing mixed with stack");
    {
        blt::gp::stack_allocator stack;
        stack.push(50.0f);
        stack.push(10.0f);
        stack.push(true);
        stack.push(false);
        
        BLT_TRACE_STREAM << stack.size() << "\n";
        basic_mixed_4.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<float>();
        RUN_TEST(val != 50.000000f, stack, "Mixed 4 Test Passed", "Mixed 4 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation.");
    }
    BLT_INFO("Testing mixed with stack over boundary");
    {
        blt::gp::stack_allocator stack;
        stack.push(std::array<blt::u8, blt::gp::stack_allocator::page_size_no_block() - sizeof(float)>{});
        stack.push(50.0f);
        stack.push(10.0f);
        stack.push(true);
        stack.push(false);
        auto size = stack.size();
        BLT_TRACE_STREAM << size << "\n";
        BLT_ASSERT(size.blocks > 1 && "Stack doesn't have more than one block!");
        basic_mixed_4.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<float>();
        stack.pop<std::array<blt::u8, blt::gp::stack_allocator::page_size_no_block() - sizeof(float)>>();
        RUN_TEST(val != 50.000000f, stack, "Mixed 4 Boundary Test Passed", "Mixed 4 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation over stack boundary");
    }
}

void test_large_256()
{
    BLT_INFO("Testing large 256 with stack");
    {
        blt::gp::stack_allocator stack;
        stack.push(base_256);
        stack.push(691.0f);
        stack.push(69.420f);
        
        BLT_TRACE_STREAM << stack.size() << "\n";
        large_256_basic_3.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_256>();
        RUN_TEST(!compare(val, base_256), stack, "Large 256 3 Test Passed", "Large 256 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation.");
    }
    BLT_INFO("Testing large 256 with stack over boundary");
    {
        blt::gp::stack_allocator stack;
        stack.push(std::array<blt::u8, blt::gp::stack_allocator::page_size_no_block() - sizeof(large_256)>{});
        stack.push(base_256);
        stack.push(691.0f);
        stack.push(69.420f);
        auto size = stack.size();
        BLT_TRACE_STREAM << size << "\n";
        BLT_ASSERT(size.blocks > 1 && "Stack doesn't have more than one block!");
        large_256_basic_3.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_256>();
        stack.pop<std::array<blt::u8, blt::gp::stack_allocator::page_size_no_block() - sizeof(large_256)>>();
        RUN_TEST(!compare(val, base_256), stack, "Large 256 3 Boundary Test Passed", "Large 256 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation over stack boundary");
    }
}

void test_large_4096()
{
    BLT_INFO("Testing large 4096 with stack");
    {
        blt::gp::stack_allocator stack;
        stack.push(base_4096);
        stack.push(33.0f);
        stack.push(true);
        BLT_TRACE_STREAM << stack.size() << "\n";
        large_4096_basic_3b.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_4096>();
        RUN_TEST(!compare(val, base_4096), stack, "Large 4096 3 Test Passed", "Large 4096 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation.");
    }
    BLT_INFO("Testing large 4096 with stack over boundary");
    {
        blt::gp::stack_allocator stack;
        stack.push(base_4096);
        stack.push(33.0f);
        stack.push(true);
        auto size = stack.size();
        BLT_TRACE_STREAM << size << "\n";
        BLT_ASSERT(size.blocks > 1 && "Stack doesn't have more than one block!");
        large_4096_basic_3b.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_4096>();
        RUN_TEST(!compare(val, base_4096), stack, "Large 4096 3 Boundary Test Passed", "Large 4096 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation over stack boundary");
    }
}

void test_large_18290()
{
    BLT_INFO("Testing large 18290 with stack");
    {
        blt::gp::stack_allocator stack;
        stack.push(base_18290);
        stack.push(-2543.0f);
        stack.push(true);
        BLT_TRACE_STREAM << stack.size() << "\n";
        large_18290_basic_3b.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_18290>();
        RUN_TEST(!compare(val, base_18290), stack, "Large 18290 3 Test Passed", "Large 4096 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation.");
    }
    BLT_INFO("Testing large 18290 with stack over boundary");
    {
        blt::gp::stack_allocator stack;
        stack.push(std::array<blt::u8, 20480 - 18290 - blt::gp::stack_allocator::block_size()>());
        stack.push(base_18290);
        stack.push(-2543.0f);
        stack.push(true);
        auto size = stack.size();
        BLT_TRACE_STREAM << size << "\n";
        BLT_ASSERT(size.blocks > 1 && "Stack doesn't have more than one block!");
        large_18290_basic_3b.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_18290>();
        stack.pop<std::array<blt::u8, 20480 - 18290 - blt::gp::stack_allocator::block_size()>>();
        RUN_TEST(!compare(val, base_18290), stack, "Large 18290 3 Boundary Test Passed", "Large 4096 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation over stack boundary");
    }
    BLT_INFO("Testing large 18290 with stack over multiple boundaries");
    {
        blt::gp::stack_allocator stack;
        stack.push(base_18290);
        stack.push(-2543.0f);
        stack.push(true);
        auto size = stack.size();
        BLT_TRACE_STREAM << size << "\n";
        large_18290_basic_3b.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        stack.push(-2543.0f);
        stack.push(true);
        BLT_TRACE_STREAM << stack.size() << "\n";
        large_18290_basic_3b.make_callable<blt::gp::detail::empty_t>()(nullptr, stack, stack, nullptr);
        BLT_TRACE_STREAM << stack.size() << "\n";
        auto val = stack.pop<large_18290>();
        RUN_TEST(!compare(val, base_18290), stack, "Large 18290 3 Boundary Test Passed", "Large 4096 3 Test Failed. Unexpected value produced '%lf'", val);
        BLT_TRACE_STREAM << stack.size() << "\n";
        BLT_ASSERT(stack.empty() && "Stack was not empty after evaluation over multiple stack boundary");
    }
}

void test_operators()
{
    log_box box("-----------------------{Operator Testing}-----------------------", BLT_INFO_STREAM);
    test_basic();
    BLT_NEWLINE();
    test_mixed();
    BLT_NEWLINE();
    test_large_256();
    BLT_NEWLINE();
    test_large_4096();
    BLT_NEWLINE();
    test_large_18290();
}

int main()
{
    test_basic_types();
    BLT_NEWLINE();
    test_operators();
}