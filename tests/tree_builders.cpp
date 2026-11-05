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
#include <string>
#include <vector>
#include <stdexcept>
#include "testing.h"

namespace
{
    using namespace blt::gp;

    void test_tree_builder_declarative_happy_paths()
    {
        std::cout << "[Test] test_tree_builder_declarative_happy_paths..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_c = []() -> float { return 10.0f; };
        auto ephem_gen = []() -> float { return 2.5f; };
        auto op_neg = [](float a) -> float { return -a; };
        auto op_add = [](float a, float b) -> float { return a + b; };
        auto op_mul = [](float a, float b) -> float { return a * b; };
        auto op_ternary = [](float a, float b, float c) -> float { return a * b + c; };

        auto r_c = f_resolver.push(term_c);
        auto r_ephem = f_resolver.push(ephem_gen, true);
        auto r_neg = f_resolver.push(op_neg);
        auto r_add = f_resolver.push(op_add);
        auto r_mul = f_resolver.push(op_mul);
        auto r_ternary = f_resolver.push(op_ternary);

        // 1. Terminal-only tree (standard terminal)
        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_c)
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 10.0f);
        }

        // 2. Terminal-only tree (ephemeral terminal via root)
        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_add, [&](auto& n) {
                    n.ephemeral(r_ephem);
                    n.child(r_c);
                })
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 12.5f);
        }

        // 3. Nested multi-level expression: ternary(add(c, ephem), neg(c), mul(c, ephem))
        // add = 10 + 2.5 = 12.5
        // neg = -10
        // mul = 10 * 2.5 = 25.0
        // ternary = 12.5 * -10 + 25.0 = -125.0 + 25.0 = -100.0
        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_ternary, [&](auto& n) {
                    n.child(r_add, [&](auto& n2) {
                        n2.child(r_c);
                        n2.ephemeral(r_ephem);
                    });
                    n.child(r_neg, [&](auto& n2) {
                        n2.child(r_c);
                    });
                    n.child(r_mul, [&](auto& n2) {
                        n2.child(r_c);
                        n2.child(r_ephem); // child() on ephemeral automatically delegates to ephemeral()
                    });
                })
                .build();

            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == -100.0f);
        }
    }

    void test_tree_builder_declarative_error_handling()
    {
        std::cout << "[Test] test_tree_builder_declarative_error_handling..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_c = []() -> float { return 5.0f; };
        auto op_neg = [](float a) -> float { return -a; };
        auto op_add = [](float a, float b) -> float { return a + b; };

        auto r_c = f_resolver.push(term_c);
        auto r_neg = f_resolver.push(op_neg);
        auto r_add = f_resolver.push(op_add);

        // 1. Calling child(non_terminal) without children callback should throw
        {
            bool threw = false;
            try
            {
                auto tree = tree_builder_t<float>{f_resolver}
                    .root(r_add, [&](auto& n) {
                        n.child(r_neg); // non-terminal used as leaf
                        n.child(r_c);
                    })
                    .build();
            }
            catch (const std::runtime_error& e)
            {
                threw = true;
                std::string msg = e.what();
                TEST_ASSERT(msg.find("not a terminal") != std::string::npos);
            }
            TEST_ASSERT(threw);
        }

        // 2. Child count mismatch: too few children for binary op (1 provided, 2 expected)
        {
            bool threw = false;
            try
            {
                auto tree = tree_builder_t<float>{f_resolver}
                    .root(r_neg, [&](auto& n) {
                        n.child(r_add, [&](auto& n2) {
                            n2.child(r_c); // only 1 child for add
                        });
                    })
                    .build();
            }
            catch (const std::runtime_error& e)
            {
                threw = true;
                std::string msg = e.what();
                TEST_ASSERT(msg.find("does not match expected number of arguments") != std::string::npos);
            }
            TEST_ASSERT(threw);
        }

        // 3. Child count mismatch: too many children for unary op (2 provided, 1 expected)
        {
            bool threw = false;
            try
            {
                auto tree = tree_builder_t<float>{f_resolver}
                    .root(r_add, [&](auto& n) {
                        n.child(r_neg, [&](auto& n2) {
                            n2.child(r_c);
                            n2.child(r_c); // 2 children for neg!
                        });
                        n.child(r_c);
                    })
                    .build();
            }
            catch (const std::runtime_error& e)
            {
                threw = true;
                std::string msg = e.what();
                TEST_ASSERT(msg.find("does not match expected number of arguments") != std::string::npos);
            }
            TEST_ASSERT(threw);
        }
    }

    void test_generative_tree_builder_happy_paths_and_states()
    {
        std::cout << "[Test] test_generative_tree_builder_happy_paths_and_states..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_c = []() -> float { return 3.0f; };
        auto ephem_c = []() -> float { return 7.0f; };
        auto op_neg = [](float a) -> float { return -a; };
        auto op_sub = [](float a, float b) -> float { return a - b; };

        auto r_c = f_resolver.push(term_c);
        auto r_ephem = f_resolver.push(ephem_c, true);
        auto r_neg = f_resolver.push(op_neg);
        auto r_sub = f_resolver.push(op_sub);

        // 1. Single leaf root
        {
            generative_tree_builder_t<float> builder{f_resolver};
            TEST_ASSERT(builder.empty());
            TEST_ASSERT(builder.pending_count() == 0);

            builder.leaf(r_c);
            TEST_ASSERT(!builder.empty());
            TEST_ASSERT(builder.pending_count() == 0);

            auto tree = builder.build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 3.0f);
        }

        // 2. Single ephemeral root
        {
            generative_tree_builder_t<float> builder{f_resolver};
            builder.ephemeral(r_ephem);
            auto tree = builder.build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 7.0f);
        }

        // 3. Composite tree: sub(ephem(7), neg(c(3))) = 7 - (-3) = 10
        {
            generative_tree_builder_t<float> builder{f_resolver};
            TEST_ASSERT(builder.empty());

            builder.begin(r_sub);
            TEST_ASSERT(builder.pending_count() == 1);

            builder.ephemeral(r_ephem);
            TEST_ASSERT(builder.pending_count() == 1);

            builder.begin(r_neg);
            TEST_ASSERT(builder.pending_count() == 2);

            builder.leaf(r_c);
            TEST_ASSERT(builder.pending_count() == 2);

            builder.end(); // closes r_neg
            TEST_ASSERT(builder.pending_count() == 1);

            builder.end(); // closes r_sub
            TEST_ASSERT(builder.pending_count() == 0);

            auto tree = builder.build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 10.0f);
        }
    }

    void test_generative_tree_builder_error_handling()
    {
        std::cout << "[Test] test_generative_tree_builder_error_handling..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_c = []() -> float { return 1.0f; };
        auto ephem_c = []() -> float { return 2.0f; };
        auto op_neg = [](float a) -> float { return -a; };
        auto op_add = [](float a, float b) -> float { return a + b; };

        auto r_c = f_resolver.push(term_c);
        auto r_ephem = f_resolver.push(ephem_c, true);
        auto r_neg = f_resolver.push(op_neg);
        auto r_add = f_resolver.push(op_add);

        // Error 1: begin() with terminal function ID
        {
            generative_tree_builder_t<float> builder{f_resolver};
            bool threw = false;
            try { builder.begin(r_c); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);

            threw = false;
            try { builder.begin(r_ephem); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 2: leaf() with non-terminal function ID
        {
            generative_tree_builder_t<float> builder{f_resolver};
            bool threw = false;
            try { builder.leaf(r_add); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 3: ephemeral() with non-terminal
        {
            generative_tree_builder_t<float> builder{f_resolver};
            bool threw = false;
            try { builder.ephemeral(r_add); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 4: ephemeral() with terminal that is NOT registered as ephemeral
        {
            generative_tree_builder_t<float> builder{f_resolver};
            bool threw = false;
            try { builder.ephemeral(r_c); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 5: end() when pending is empty
        {
            generative_tree_builder_t<float> builder{f_resolver};
            bool threw = false;
            try { builder.end(); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 6: end() with child count mismatch (too few children)
        {
            generative_tree_builder_t<float> builder{f_resolver};
            builder.begin(r_add);
            builder.leaf(r_c); // only 1 of 2 children
            bool threw = false;
            try { builder.end(); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 7: count_child() with too many children emitted
        {
            generative_tree_builder_t<float> builder{f_resolver};
            builder.begin(r_neg); // expects 1 child
            builder.leaf(r_c);
            bool threw = false;
            try { builder.leaf(r_c); } // 2nd child!
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 8: build() with unclosed pending nodes
        {
            generative_tree_builder_t<float> builder{f_resolver};
            builder.begin(r_neg);
            builder.leaf(r_c);
            bool threw = false;
            try { builder.build(); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 9: build() without emitting any root
        {
            generative_tree_builder_t<float> builder{f_resolver};
            bool threw = false;
            try { builder.build(); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        // Error 10: multiple root emissions
        {
            generative_tree_builder_t<float> builder{f_resolver};
            builder.leaf(r_c); // root complete
            bool threw = false;
            try { builder.leaf(r_c); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }

        {
            generative_tree_builder_t<float> builder{f_resolver};
            builder.leaf(r_c); // root complete
            bool threw = false;
            try { builder.begin(r_neg); }
            catch (const std::runtime_error& e) { threw = true; }
            TEST_ASSERT(threw);
        }
    }

    void test_cross_builder_equivalence()
    {
        std::cout << "[Test] test_cross_builder_equivalence..." << std::endl;

        function_resolver_t<double> f_resolver;

        auto term_a = []() -> double { return 12.0; };
        auto ephem_b = []() -> double { return 4.0; };
        auto op_sub = [](double a, double b) -> double { return a - b; };
        auto op_mul = [](double a, double b) -> double { return a * b; };

        auto r_a = f_resolver.push(term_a);
        auto r_b = f_resolver.push(ephem_b, true);
        auto r_sub = f_resolver.push(op_sub);
        auto r_mul = f_resolver.push(op_mul);

        // Expression: mul(sub(a, ephem_b), sub(ephem_b, a))
        // mul(12 - 4, 4 - 12) = 8 * -8 = -64

        // 1. Build via tree_builder_t
        auto tree1 = tree_builder_t<double>{f_resolver}
            .root(r_mul, [&](auto& n) {
                n.child(r_sub, [&](auto& n2) {
                    n2.child(r_a);
                    n2.ephemeral(r_b);
                });
                n.child(r_sub, [&](auto& n2) {
                    n2.ephemeral(r_b);
                    n2.child(r_a);
                });
            })
            .build();

        // 2. Build via generative_tree_builder_t
        generative_tree_builder_t<double> builder{f_resolver};
        builder.begin(r_mul);
        {
            builder.begin(r_sub);
            {
                builder.leaf(r_a);
                builder.ephemeral(r_b);
            }
            builder.end();

            builder.begin(r_sub);
            {
                builder.ephemeral(r_b);
                builder.leaf(r_a);
            }
            builder.end();
        }
        builder.end();
        auto tree2 = builder.build();

        // Check equivalence of operators
        TEST_ASSERT(tree1.root().size() == tree2.root().size());
        TEST_ASSERT(tree1.root().size() == 7); // [a, b, sub, b, a, sub, mul]

        // Check evaluated results
        auto val1 = tree1.evaluate(f_resolver);
        auto val2 = tree2.evaluate(f_resolver);

        TEST_ASSERT(val1 == -64.0);
        TEST_ASSERT(val2 == -64.0);
        TEST_ASSERT(val1 == val2);
    }
}

int main()
{
    std::cout << "============================================" << std::endl;
    std::cout << " Running gp-tree-builders Unit Test Suite   " << std::endl;
    std::cout << "============================================" << std::endl;

    test_tree_builder_declarative_happy_paths();
    test_tree_builder_declarative_error_handling();
    test_generative_tree_builder_happy_paths_and_states();
    test_generative_tree_builder_error_handling();
    test_cross_builder_equivalence();

    std::cout << "============================================" << std::endl;
    std::cout << " All gp-tree-builders tests PASSED!         " << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
