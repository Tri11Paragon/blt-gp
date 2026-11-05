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

    void test_function_info()
    {
        std::cout << "[Test] test_function_info..." << std::endl;

        // Terminal function_info (no args)
        function_info_t term_info{function_id_t{0}, 0, false};
        TEST_ASSERT(term_info.id.get() == 0);
        TEST_ASSERT(term_info.is_terminal());
        TEST_ASSERT(term_info.argument_count == 0);
        TEST_ASSERT(!term_info.stores_in_stack);

        // Non-terminal function_info (2 args)
        function_info_t non_term_info{function_id_t{1}, 2, false};
        TEST_ASSERT(non_term_info.id.get() == 1);
        TEST_ASSERT(!non_term_info.is_terminal());
        TEST_ASSERT(non_term_info.argument_count == 2);
        TEST_ASSERT(!non_term_info.stores_in_stack);
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

        function_resolver_t<float> f_resolver;

        auto term_fn = []() -> float { return 42.0f; };
        auto unary_fn = [](float a) -> float { return -a; };
        auto binary_fn = [](float a, float b) -> float { return a + b; };
        auto ephem_fn = []() -> float { return 3.14159f; };

        FunctorOp functor{2.5f};

        // Register functions
        auto f_term = f_resolver.push(term_fn);
        auto f_unary = f_resolver.push(unary_fn);
        auto f_binary = f_resolver.push(binary_fn);
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
        TEST_ASSERT(info_term.argument_count == 0);

        const auto& info_unary = f_resolver.get(f_unary);
        TEST_ASSERT(!info_unary.is_terminal());
        TEST_ASSERT(info_unary.argument_count == 1);

        const auto& info_binary = f_resolver.get(f_binary);
        TEST_ASSERT(!info_binary.is_terminal());
        TEST_ASSERT(info_binary.argument_count == 2);

        // Check ephemeral flags
        TEST_ASSERT(!f_resolver.stores_in_stack(f_term));
        TEST_ASSERT(!f_resolver.stores_in_stack(f_unary));
        TEST_ASSERT(!f_resolver.stores_in_stack(f_binary));
        TEST_ASSERT(f_resolver.stores_in_stack(f_ephem));
        TEST_ASSERT(!f_resolver.stores_in_stack(f_functor));

        // Check string representation
        std::string str_term = f_resolver.to_string(f_term);
        TEST_ASSERT(!str_term.empty());

        std::string str_binary = f_resolver.to_string(f_binary);
        TEST_ASSERT(!str_binary.empty());

        // Test forward evaluation via function_resolver_t
        {
            std::vector<float> in_stack, out_stack;
            f_resolver.forward(f_term, in_stack, out_stack);
            TEST_ASSERT(!out_stack.empty());
            TEST_ASSERT(out_stack.back() == 42.0f);
        }

        {
            std::vector<float> in_stack{10.5f}, out_stack;
            f_resolver.forward(f_unary, in_stack, out_stack);
            TEST_ASSERT(!out_stack.empty());
            TEST_ASSERT(out_stack.back() == -10.5f);
        }

        {
            std::vector<float> in_stack{15.5f, 10.0f}, out_stack;
            f_resolver.forward(f_binary, in_stack, out_stack);
            TEST_ASSERT(!out_stack.empty());
            TEST_ASSERT(out_stack.back() == 25.5f);
        }

        {
            std::vector<float> in_stack{3.0f, 7.0f}, out_stack;
            f_resolver.forward(f_functor, in_stack, out_stack);
            TEST_ASSERT(!out_stack.empty());
            TEST_ASSERT(out_stack.back() == (3.0f + 7.0f) * 2.5f);
        }
    }
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " Running gp-tree-types Unit Test Suite  " << std::endl;
    std::cout << "========================================" << std::endl;

    test_function_info();
    test_function_resolver();

    std::cout << "========================================" << std::endl;
    std::cout << " All gp-tree-types tests PASSED!        " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
