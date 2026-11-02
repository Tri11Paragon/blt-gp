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

#include <blt/gp/tree.h>
#include <iostream>
#include <cstdint>
#include <string>
#include <vector>
#include <functional>
#include "testing.h"

namespace
{
    using namespace blt::gp;

    // Custom POD structures of varied sizes and alignments
    struct alignas(1) Pod1
    {
        uint8_t a;
    };

    struct alignas(1) Pod2
    {
        uint8_t a, b;
    };

    struct alignas(1) Pod3
    {
        uint8_t a, b, c;
    };

    struct alignas(1) Pod5
    {
        uint8_t data[5];
    };

    struct alignas(1) Pod7
    {
        uint8_t data[7];
    };

    struct Pod12
    {
        int32_t a, b, c;
    };

    struct Pod16
    {
        uint64_t a, b;
    };

    struct Pod24
    {
        uint64_t a, b, c;
    };

    void test_type_id_and_info()
    {
        std::cout << "[Test] test_type_id_and_info..." << std::endl;

        type_id_t id0;
        TEST_ASSERT(id0.get() == 0);
        TEST_ASSERT(static_cast<size_t>(id0) == 0);

        type_id_t id42{42};
        TEST_ASSERT(id42.get() == 42);
        TEST_ASSERT(static_cast<size_t>(id42) == 42);

        // type_info_t manual construction & build
        auto info_int = type_info_t::build<int>(type_id_t{1});
        TEST_ASSERT(info_int.id.get() == 1);
        TEST_ASSERT(info_int.size == sizeof(int));
        TEST_ASSERT(info_int.aligned_size == detail::align_bytes(sizeof(int)));
        TEST_ASSERT(info_int.aligned_size == 8);

        auto info_pod1 = type_info_t::build<Pod1>(type_id_t{2});
        TEST_ASSERT(info_pod1.id.get() == 2);
        TEST_ASSERT(info_pod1.size == 1);
        TEST_ASSERT(info_pod1.aligned_size == 8);

        auto info_pod3 = type_info_t::build<Pod3>(type_id_t{3});
        TEST_ASSERT(info_pod3.size == 3);
        TEST_ASSERT(info_pod3.aligned_size == 8);

        auto info_pod7 = type_info_t::build<Pod7>(type_id_t{4});
        TEST_ASSERT(info_pod7.size == 7);
        TEST_ASSERT(info_pod7.aligned_size == 8);

        auto info_pod12 = type_info_t::build<Pod12>(type_id_t{5});
        TEST_ASSERT(info_pod12.size == 12);
        TEST_ASSERT(info_pod12.aligned_size == 16);

        auto info_pod16 = type_info_t::build<Pod16>(type_id_t{6});
        TEST_ASSERT(info_pod16.size == 16);
        TEST_ASSERT(info_pod16.aligned_size == 16);

        auto info_pod24 = type_info_t::build<Pod24>(type_id_t{7});
        TEST_ASSERT(info_pod24.size == 24);
        TEST_ASSERT(info_pod24.aligned_size == 24);
    }

    void test_type_resolver()
    {
        std::cout << "[Test] test_type_resolver..." << std::endl;

        type_resolver_t resolver;

        // Register fundamental types
        auto t_int = resolver.register_type<int>();
        auto t_float = resolver.register_type<float>();
        auto t_double = resolver.register_type<double>();
        auto t_bool = resolver.register_type<bool>();
        auto t_size_t = resolver.register_type<size_t>();
        auto t_char = resolver.register_type<char>();
        auto t_uint8 = resolver.register_type<uint8_t>();
        auto t_int16 = resolver.register_type<int16_t>();
        auto t_int32 = resolver.register_type<int32_t>();

        TEST_ASSERT(t_int.id.get() == 0);
        TEST_ASSERT(t_float.id.get() == 1);
        TEST_ASSERT(t_double.id.get() == 2);
        TEST_ASSERT(t_bool.id.get() == 3);
        TEST_ASSERT(t_size_t.id.get() == 4);
        TEST_ASSERT(t_char.id.get() == 5);
        TEST_ASSERT(t_uint8.id.get() == 6);
        TEST_ASSERT(t_int16.id.get() == 7);
        // int32_t is int on x86_64, so it resolves to id 0
        TEST_ASSERT(t_int32.id.get() == t_int.id.get());

        // Deduplication: registering again returns same ID
        auto t_int_again = resolver.register_type<int>();
        TEST_ASSERT(t_int_again.id.get() == t_int.id.get());
        TEST_ASSERT(t_int_again.size == sizeof(int));
        TEST_ASSERT(t_int_again.aligned_size == detail::align_bytes(sizeof(int)));

        auto t_float_again = resolver.register_type<float>();
        TEST_ASSERT(t_float_again.id.get() == t_float.id.get());

        // Register custom POD types
        auto t_p1 = resolver.register_type<Pod1>();
        auto t_p2 = resolver.register_type<Pod2>();
        auto t_p3 = resolver.register_type<Pod3>();
        auto t_p5 = resolver.register_type<Pod5>();
        auto t_p7 = resolver.register_type<Pod7>();
        auto t_p12 = resolver.register_type<Pod12>();
        auto t_p16 = resolver.register_type<Pod16>();
        auto t_p24 = resolver.register_type<Pod24>();

        TEST_ASSERT(t_p1.size == 1 && t_p1.aligned_size == 8);
        TEST_ASSERT(t_p2.size == 2 && t_p2.aligned_size == 8);
        TEST_ASSERT(t_p3.size == 3 && t_p3.aligned_size == 8);
        TEST_ASSERT(t_p5.size == 5 && t_p5.aligned_size == 8);
        TEST_ASSERT(t_p7.size == 7 && t_p7.aligned_size == 8);
        TEST_ASSERT(t_p12.size == 12 && t_p12.aligned_size == 16);
        TEST_ASSERT(t_p16.size == 16 && t_p16.aligned_size == 16);
        TEST_ASSERT(t_p24.size == 24 && t_p24.aligned_size == 24);

        // Querying type IDs and info
        TEST_ASSERT(resolver.get<int>().get() == t_int.id.get());
        TEST_ASSERT(resolver.get<Pod12>().get() == t_p12.id.get());
        TEST_ASSERT(resolver.get_info<Pod12>().size == 12);
        TEST_ASSERT(resolver.get_info<Pod12>().aligned_size == 16);
        TEST_ASSERT(resolver.get_info(t_p24.id).size == 24);
        TEST_ASSERT(resolver.get_info(t_p24.id).aligned_size == 24);

        // Querying type names
        const auto& name_int = resolver.get_name(t_int.id);
        TEST_ASSERT(!name_int.empty());
        const auto& name_p12 = resolver.get_name(t_p12.id);
        TEST_ASSERT(!name_p12.empty());
    }

    void test_function_info()
    {
        std::cout << "[Test] test_function_info..." << std::endl;

        type_resolver_t resolver;
        auto t_float = resolver.register_type<float>();
        auto t_int = resolver.register_type<int>();

        // Terminal function_info (no args)
        function_info_t<std::vector> term_info{function_id_t{0}, t_float, {}, false};
        TEST_ASSERT(term_info.id.get() == 0);
        TEST_ASSERT(term_info.return_type.id.get() == t_float.id.get());
        TEST_ASSERT(term_info.is_terminal());
        TEST_ASSERT(term_info.argument_types.empty());

        // Non-terminal function_info (2 args)
        function_info_t<std::vector> non_term_info{function_id_t{1}, t_float, {t_float, t_int}, false};
        TEST_ASSERT(non_term_info.id.get() == 1);
        TEST_ASSERT(non_term_info.return_type.id.get() == t_float.id.get());
        TEST_ASSERT(!non_term_info.is_terminal());
        TEST_ASSERT(non_term_info.argument_types.size() == 2);
        TEST_ASSERT(non_term_info.argument_types[0].id.get() == t_float.id.get());
        TEST_ASSERT(non_term_info.argument_types[1].id.get() == t_int.id.get());
    }

    struct FunctorOp
    {
        float scale;
        float operator()(float a, float b) const
        {
            return (a + b) * scale;
        }
    };

    void test_function_resolver()
    {
        std::cout << "[Test] test_function_resolver..." << std::endl;

        type_resolver_t t_resolver;
        function_resolver_t f_resolver{t_resolver};

        auto term_fn = []() -> float { return 42.0f; };
        auto unary_fn = [](float a) -> float { return -a; };
        auto binary_fn = [](float a, int b) -> double { return static_cast<double>(a) + b; };
        auto anti_binary_fn = [](double d) -> float { return static_cast<float>(d * 2.0); };
        auto ephem_fn = []() -> float { return 3.14159f; };

        FunctorOp functor{2.5f};

        // Register functions
        auto f_term = f_resolver.push(term_fn);
        auto f_unary = f_resolver.push(unary_fn);
        auto f_binary = f_resolver.push(binary_fn, anti_binary_fn);
        auto f_ephem = f_resolver.push(ephem_fn, true);
        auto f_functor = f_resolver.push(functor);

        // Verification of IDs
        TEST_ASSERT(f_term.get() == 0);
        TEST_ASSERT(f_unary.get() == 1);
        TEST_ASSERT(f_binary.get() == 2);
        TEST_ASSERT(f_ephem.get() == 3);
        TEST_ASSERT(f_functor.get() == 4);

        // Check terminal status and argument types
        const auto& info_term = f_resolver.get(f_term);
        TEST_ASSERT(info_term.is_terminal());
        TEST_ASSERT(info_term.argument_types.empty());

        const auto& info_unary = f_resolver.get(f_unary);
        TEST_ASSERT(!info_unary.is_terminal());
        TEST_ASSERT(info_unary.argument_types.size() == 1);

        const auto& info_binary = f_resolver.get(f_binary);
        TEST_ASSERT(!info_binary.is_terminal());
        TEST_ASSERT(info_binary.argument_types.size() == 2);

        // Check backward callback metadata
        TEST_ASSERT(!f_resolver.has_backwards(f_term));
        TEST_ASSERT(!f_resolver.has_backwards(f_unary));
        TEST_ASSERT(f_resolver.has_backwards(f_binary));
        TEST_ASSERT(!f_resolver.has_backwards(f_ephem));
        TEST_ASSERT(!f_resolver.has_backwards(f_functor));

        // Check ephemeral flags
        TEST_ASSERT(!f_resolver.stores_in_stack(f_term));
        TEST_ASSERT(!f_resolver.stores_in_stack(f_unary));
        TEST_ASSERT(!f_resolver.stores_in_stack(f_binary));
        TEST_ASSERT(f_resolver.stores_in_stack(f_ephem));
        TEST_ASSERT(!f_resolver.stores_in_stack(f_functor));

        // Check string representation
        std::string str_term = f_resolver.to_string(f_term);
        TEST_ASSERT(!str_term.empty());
        TEST_ASSERT(str_term.find("=> () ->") != std::string::npos);

        std::string str_binary = f_resolver.to_string(f_binary);
        TEST_ASSERT(!str_binary.empty());
        TEST_ASSERT(str_binary.find("=> (") != std::string::npos);

        // Test forward evaluation via function_resolver_t
        {
            byte_stack_t in_stack, out_stack;
            f_resolver.forward(f_term, in_stack, out_stack);
            TEST_ASSERT(out_stack.pop<float>() == 42.0f);
        }

        {
            byte_stack_t in_stack, out_stack;
            in_stack.push(10.5f);
            f_resolver.forward(f_unary, in_stack, out_stack);
            TEST_ASSERT(out_stack.pop<float>() == -10.5f);
        }

        {
            byte_stack_t in_stack, out_stack;
            // binary_fn takes (float a, int b)
            in_stack.push(15.5f);
            in_stack.push(10);
            f_resolver.forward(f_binary, in_stack, out_stack);
            TEST_ASSERT(out_stack.pop<double>() == 25.5);
        }

        {
            byte_stack_t in_stack, out_stack;
            in_stack.push(3.0f);
            in_stack.push(7.0f);
            f_resolver.forward(f_functor, in_stack, out_stack);
            TEST_ASSERT(out_stack.pop<float>() == (3.0f + 7.0f) * 2.5f);
        }

        // Test backward evaluation
        {
            byte_stack_t in_stack, out_stack;
            in_stack.push(12.0);
            f_resolver.backward(f_binary, in_stack, out_stack);
            TEST_ASSERT(out_stack.pop<float>() == 24.0f);

            // Backward on function without backward callback should be no-op
            byte_stack_t dummy_in, dummy_out;
            f_resolver.backward(f_unary, dummy_in, dummy_out);
            TEST_ASSERT(dummy_out.empty());
        }
    }
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " Running gp-tree-types Unit Test Suite  " << std::endl;
    std::cout << "========================================" << std::endl;

    test_type_id_and_info();
    test_type_resolver();
    test_function_info();
    test_function_resolver();

    std::cout << "========================================" << std::endl;
    std::cout << " All gp-tree-types tests PASSED!        " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
