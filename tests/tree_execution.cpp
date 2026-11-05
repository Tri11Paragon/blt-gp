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
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
#include "testing.h"

namespace
{
    using namespace blt::gp;

    // Irregular POD types to stress 8-byte alignment & packing
    struct alignas(1) Pod1
    {
        uint8_t a;
    };

    struct alignas(1) Pod3
    {
        uint8_t a, b, c;
    };

    struct alignas(1) Pod7
    {
        uint8_t data[7];
    };

    struct Pod12
    {
        int32_t a, b, c;
    };

    struct Pod24
    {
        uint64_t a, b, c;
    };

    void test_unary_math_and_projections()
    {
        std::cout << "[Test] test_unary_math_and_projections..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_val = []() -> float { return 4.0f; };

        auto op_neg = [](float x) -> float { return -x; };
        auto op_sqrt = [](float x) -> float { return std::sqrt(x); };
        auto op_sin = [](float x) -> float { return std::sin(x); };
        auto op_cos = [](float x) -> float { return std::cos(x); };
        auto op_log = [](float x) -> float { return std::log(x); };
        auto op_abs = [](float x) -> float { return std::abs(x); };

        auto r_term = f_resolver.push(term_val);
        auto r_neg = f_resolver.push(op_neg);
        auto r_sqrt = f_resolver.push(op_sqrt);
        auto r_sin = f_resolver.push(op_sin);
        auto r_cos = f_resolver.push(op_cos);
        auto r_log = f_resolver.push(op_log);
        auto r_abs = f_resolver.push(op_abs);

        // sqrt(4.0) = 2.0
        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_sqrt, [&](auto& n) {
                    n.child(r_term);
                })
                .build();
            auto result = tree.evaluate(f_resolver);
            TEST_ASSERT(result == 2.0f);
        }

        // neg(sqrt(4.0)) = -2.0
        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_neg, [&](auto& n) {
                    n.child(r_sqrt, [&](auto& n2) {
                        n2.child(r_term);
                    });
                })
                .build();
            auto result = tree.evaluate(f_resolver);
            TEST_ASSERT(result == -2.0f);
        }

        // abs(neg(4.0)) = 4.0
        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_abs, [&](auto& n) {
                    n.child(r_neg, [&](auto& n2) {
                        n2.child(r_term);
                    });
                })
                .build();
            auto result = tree.evaluate(f_resolver);
            TEST_ASSERT(result == 4.0f);
        }

        // sin(0.0) or cos(0.0), log(e)
        auto term_zero = []() -> float { return 0.0f; };
        auto term_e = []() -> float { return static_cast<float>(M_E); };
        auto r_zero = f_resolver.push(term_zero);
        auto r_e = f_resolver.push(term_e);

        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_log, [&](auto& n) {
                    n.child(r_e);
                })
                .build();
            auto result = tree.evaluate(f_resolver);
            TEST_ASSERT(std::abs(result - 1.0f) < 1e-5f);
        }

        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_cos, [&](auto& n) {
                    n.child(r_zero);
                })
                .build();
            auto result = tree.evaluate(f_resolver);
            TEST_ASSERT(result == 1.0f);
        }

        {
            auto tree = tree_builder_t<float>{f_resolver}
                .root(r_sin, [&](auto& n) {
                    n.child(r_zero);
                })
                .build();
            auto result = tree.evaluate(f_resolver);
            TEST_ASSERT(result == 0.0f);
        }

        // Custom struct type tree evaluation
        {
            function_resolver_t<Pod12> pod_resolver;
            auto term_pod = []() -> Pod12 { return Pod12{10, 20, 30}; };
            auto op_swap_ac = [](Pod12 p) -> Pod12 { return Pod12{p.c, p.b, p.a}; };
            auto r_pod = pod_resolver.push(term_pod);
            auto r_swap = pod_resolver.push(op_swap_ac);

            auto tree = tree_builder_t<Pod12>{pod_resolver}
                .root(r_swap, [&](auto& n) {
                    n.child(r_pod);
                })
                .build();
            auto res = tree.evaluate(pod_resolver);
            TEST_ASSERT(res.a == 30 && res.b == 20 && res.c == 10);
        }
    }

    void test_homogeneous_multi_arg_and_ordering()
    {
        std::cout << "[Test] test_homogeneous_multi_arg_and_ordering..." << std::endl;

        function_resolver_t<double> f_resolver;

        auto val_1 = []() -> double { return 1.0; };
        auto val_2 = []() -> double { return 2.0; };
        auto val_3 = []() -> double { return 3.0; };
        auto val_4 = []() -> double { return 4.0; };
        auto val_5 = []() -> double { return 5.0; };
        auto val_100 = []() -> double { return 100.0; };
        auto val_20 = []() -> double { return 20.0; };

        auto r_1 = f_resolver.push(val_1);
        auto r_2 = f_resolver.push(val_2);
        auto r_3 = f_resolver.push(val_3);
        auto r_4 = f_resolver.push(val_4);
        auto r_5 = f_resolver.push(val_5);
        auto r_100 = f_resolver.push(val_100);
        auto r_20 = f_resolver.push(val_20);

        // Binary non-commutative
        auto op_sub = [](double a, double b) -> double { return a - b; };
        auto op_div = [](double a, double b) -> double { return a / b; };
        auto r_sub = f_resolver.push(op_sub);
        auto r_div = f_resolver.push(op_div);

        // Ternary non-commutative: a * 100 + b * 10 + c
        auto op_order3 = [](double a, double b, double c) -> double {
            return a * 100.0 + b * 10.0 + c;
        };
        auto r_order3 = f_resolver.push(op_order3);

        // 4-arg non-commutative: a * 1000 + b * 100 + c * 10 + d
        auto op_order4 = [](double a, double b, double c, double d) -> double {
            return a * 1000.0 + b * 100.0 + c * 10.0 + d;
        };
        auto r_order4 = f_resolver.push(op_order4);

        // 5-arg non-commutative: a * 10000 + b * 1000 + c * 100 + d * 10 + e
        auto op_order5 = [](double a, double b, double c, double d, double e) -> double {
            return a * 10000.0 + b * 1000.0 + c * 100.0 + d * 10.0 + e;
        };
        auto r_order5 = f_resolver.push(op_order5);

        // Check sub(100, 20) = 80 (not -80)
        {
            auto tree = tree_builder_t<double>{f_resolver}
                .root(r_sub, [&](auto& n) {
                    n.child(r_100);
                    n.child(r_20);
                })
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 80.0);
        }

        // Check div(100, 20) = 5.0 (not 0.2)
        {
            auto tree = tree_builder_t<double>{f_resolver}
                .root(r_div, [&](auto& n) {
                    n.child(r_100);
                    n.child(r_20);
                })
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 5.0);
        }

        // Check order3(1, 2, 3) = 123
        {
            auto tree = tree_builder_t<double>{f_resolver}
                .root(r_order3, [&](auto& n) {
                    n.child(r_1);
                    n.child(r_2);
                    n.child(r_3);
                })
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 123.0);
        }

        // Check order4(1, 2, 3, 4) = 1234
        {
            auto tree = tree_builder_t<double>{f_resolver}
                .root(r_order4, [&](auto& n) {
                    n.child(r_1);
                    n.child(r_2);
                    n.child(r_3);
                    n.child(r_4);
                })
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 1234.0);
        }

        // Check order5(1, 2, 3, 4, 5) = 12345
        {
            auto tree = tree_builder_t<double>{f_resolver}
                .root(r_order5, [&](auto& n) {
                    n.child(r_1);
                    n.child(r_2);
                    n.child(r_3);
                    n.child(r_4);
                    n.child(r_5);
                })
                .build();
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 12345.0);
        }
    }

    void test_custom_types_trees()
    {
        std::cout << "[Test] test_custom_types_trees..." << std::endl;

        // Test int32_t tree
        {
            function_resolver_t<int32_t> int_resolver;
            auto val_10 = []() -> int32_t { return 10; };
            auto val_25 = []() -> int32_t { return 25; };
            auto op_add = [](int32_t a, int32_t b) -> int32_t { return a + b; };
            auto op_mul = [](int32_t a, int32_t b) -> int32_t { return a * b; };

            auto r_10 = int_resolver.push(val_10);
            auto r_25 = int_resolver.push(val_25);
            auto r_add = int_resolver.push(op_add);
            auto r_mul = int_resolver.push(op_mul);

            auto tree = tree_builder_t<int32_t>{int_resolver}
                .root(r_mul, [&](auto& n) {
                    n.child(r_10);
                    n.child(r_add, [&](auto& n2) {
                        n2.child(r_10);
                        n2.child(r_25);
                    });
                })
                .build();
            auto res = tree.evaluate(int_resolver);
            TEST_ASSERT(res == 10 * (10 + 25)); // 350
        }
    }

    void test_ephemeral_evaluation_stability()
    {
        std::cout << "[Test] test_ephemeral_evaluation_stability..." << std::endl;

        function_resolver_t<float> f_resolver;

        int call_count = 0;
        auto ephem_counter = [&]() -> float {
            ++call_count;
            return static_cast<float>(call_count * 10);
        };

        auto r_ephem = f_resolver.push(ephem_counter, true);

        auto op_add = [](float a, float b) -> float { return a + b; };
        auto r_add = f_resolver.push(op_add);

        // Construct tree with 2 ephemeral leaves
        // Leaf 1 gets 10, Leaf 2 gets 20 -> tree computes 10 + 20 = 30
        auto tree = tree_builder_t<float>{f_resolver}
            .root(r_add, [&](auto& n) {
                n.ephemeral(r_ephem);
                n.ephemeral(r_ephem);
            })
            .build();

        TEST_ASSERT(call_count == 2);

        // Repeated evaluate calls MUST evaluate to 30.0 and NOT call generator again
        for (int i = 0; i < 20; ++i)
        {
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 30.0f);
        }

        // Generator count should still be 2!
        TEST_ASSERT(call_count == 2);
    }
}

int main()
{
    std::cout << "============================================" << std::endl;
    std::cout << " Running gp-tree-execution Unit Test Suite  " << std::endl;
    std::cout << "============================================" << std::endl;

    test_unary_math_and_projections();
    test_homogeneous_multi_arg_and_ordering();
    test_custom_types_trees();
    test_ephemeral_evaluation_stability();

    std::cout << "============================================" << std::endl;
    std::cout << " All gp-tree-execution tests PASSED!        " << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
