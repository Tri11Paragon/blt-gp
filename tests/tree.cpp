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
#include <vector>
#include <cmath>
#include "testing.h"

using namespace blt::gp;

namespace
{
    struct SampleData
    {
        float x;
        float y;
        float expected;
    };

    void test_end_to_end_gp_crossover_and_mutation()
    {
        std::cout << "[Test] test_end_to_end_gp_crossover_and_mutation..." << std::endl;

        function_resolver_t<float, std::allocator> f_resolver;

        float input_x = 0.0f;
        float input_y = 0.0f;

        auto get_x = [&]() -> float { return input_x; };
        auto get_y = [&]() -> float { return input_y; };

        float ephem_seed = 1.0f;
        auto ephem_val = [&]() -> float {
            ephem_seed += 0.5f;
            return ephem_seed;
        };

        auto op_add = [](float a, float b) -> float { return a + b; };
        auto op_sub = [](float a, float b) -> float { return a - b; };
        auto op_mul = [](float a, float b) -> float { return a * b; };
        auto op_neg = [](float a) -> float { return -a; };

        auto r_x = f_resolver.push(get_x);
        auto r_y = f_resolver.push(get_y);
        auto r_ephem = f_resolver.push(ephem_val, true);
        auto r_add = f_resolver.push(op_add);
        auto r_sub = f_resolver.push(op_sub);
        auto r_mul = f_resolver.push(op_mul);
        auto r_neg = f_resolver.push(op_neg);

        // Parent 1: add(mul(x, ephem_val:1.5), sub(y, ephem_val:2.0))
        // P1(x, y) = (x * 1.5) + (y - 2.0)
        auto parent1 = tree_builder_t{f_resolver}
            .root(r_add, [&](auto& n) {
                n.child(r_mul, [&](auto& n2) {
                    n2.child(r_x);
                    n2.ephemeral(r_ephem);
                });
                n.child(r_sub, [&](auto& n2) {
                    n2.child(r_y);
                    n2.ephemeral(r_ephem);
                });
            })
            .build();

        // Parent 2 (using generative builder): mul(neg(y), add(x, ephem_val:2.5))
        // P2(x, y) = (-y) * (x + 2.5)
        generative_tree_builder_t gen_builder{f_resolver};
        gen_builder.begin(r_mul);
        {
            gen_builder.begin(r_neg);
            {
                gen_builder.leaf(r_y);
            }
            gen_builder.end();

            gen_builder.begin(r_add);
            {
                gen_builder.leaf(r_x);
                gen_builder.ephemeral(r_ephem);
            }
            gen_builder.end();
        }
        gen_builder.end();
        auto parent2 = gen_builder.build();

        // Evaluate Parent 1 on (x=4, y=10): (4 * 1.5) + (10 - 2.0) = 6 + 8 = 14
        input_x = 4.0f;
        input_y = 10.0f;
        auto stack_p1 = parent1.evaluate(f_resolver);
        TEST_ASSERT(stack_p1 == 14.0f);

        // Evaluate Parent 2 on (x=4, y=10): (-10) * (4 + 2.5) = -10 * 6.5 = -65
        auto stack_p2 = parent2.evaluate(f_resolver);
        TEST_ASSERT(stack_p2 == -65.0f);

        // Crossover: Take child 0 of parent 2 (which is add(x, 2.5)) and replace child 0 of parent 1 (which is sub(y, 2.0))
        // In parent 1: root is add(mul(x, 1.5), sub(y, 2.0)). child(0) in reverse postorder is sub(y, 2.0).
        // In parent 2: root is mul(neg(y), add(x, 2.5)). child(0) is add(x, 2.5).
        auto p2_subtree = parent2.root().child(0).to_tree();
        TEST_ASSERT(p2_subtree.root().size() == 3);

        auto offspring = parent1; // copy parent 1
        offspring.replace(offspring.root().child(0), p2_subtree);

        // Offspring is now: add(mul(x, 1.5), add(x, 2.5))
        // For (x=4, y=10): (4 * 1.5) + (4 + 2.5) = 6 + 6.5 = 12.5
        auto stack_off = offspring.evaluate(f_resolver);
        TEST_ASSERT(stack_off == 12.5f);

        // Point Mutation: replace the leaf 'x' in mul(x, 1.5) with 'y'
        // child(1) of offspring root is mul(x, 1.5). In mul(x, 1.5), child(1) is 'x'.
        auto leaf_y = tree_builder_t<float>{f_resolver}.root(r_y).build();
        offspring.replace(offspring.root().child(1).child(1), leaf_y);

        // Offspring after mutation: add(mul(y, 1.5), add(x, 2.5))
        // For (x=4, y=10): (10 * 1.5) + (4 + 2.5) = 15 + 6.5 = 21.5
        auto stack_mut = offspring.evaluate(f_resolver);
        TEST_ASSERT(stack_mut == 21.5f);

        // Test stability across multiple input cases
        std::vector<SampleData> dataset = {
            {0.0f, 0.0f, (0.0f * 1.5f) + (0.0f + 2.5f)}, // 2.5
            {2.0f, 4.0f, (4.0f * 1.5f) + (2.0f + 2.5f)}, // 6 + 4.5 = 10.5
            {-5.0f, 8.0f, (8.0f * 1.5f) + (-5.0f + 2.5f)} // 12 - 2.5 = 9.5
        };

        for (const auto& sample : dataset)
        {
            input_x = sample.x;
            input_y = sample.y;
            auto res = offspring.evaluate(f_resolver);
            TEST_ASSERT(std::abs(res - sample.expected) < 1e-5f);
        }
    }
}

int main()
{
    std::cout << "========================================" << std::endl;
    std::cout << " Running gp-tree Integration Test Suite " << std::endl;
    std::cout << "========================================" << std::endl;

    test_end_to_end_gp_crossover_and_mutation();

    std::cout << "========================================" << std::endl;
    std::cout << " All gp-tree tests PASSED!              " << std::endl;
    std::cout << "========================================" << std::endl;

    return 0;
}
