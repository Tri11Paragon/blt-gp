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

        type_resolver_t t_resolver;
        function_resolver_t f_resolver{t_resolver};

        auto term_val = []() -> float { return 4.0f; };
        auto term_pod = []() -> Pod12 { return Pod12{10, 20, 30}; };

        auto op_neg = [](float x) -> float { return -x; };
        auto op_sqrt = [](float x) -> float { return std::sqrt(x); };
        auto op_sin = [](float x) -> float { return std::sin(x); };
        auto op_cos = [](float x) -> float { return std::cos(x); };
        auto op_log = [](float x) -> float { return std::log(x); };
        auto op_abs = [](float x) -> float { return std::abs(x); };
        auto op_extract_b = [](Pod12 p) -> int32_t { return p.b; };
        auto op_extract_c = [](Pod12 p) -> int32_t { return p.c; };
        auto op_identity = [](int32_t x) -> int32_t { return x; };

        auto r_term = f_resolver.push(term_val);
        auto r_pod = f_resolver.push(term_pod);
        auto r_neg = f_resolver.push(op_neg);
        auto r_sqrt = f_resolver.push(op_sqrt);
        auto r_sin = f_resolver.push(op_sin);
        auto r_cos = f_resolver.push(op_cos);
        auto r_log = f_resolver.push(op_log);
        auto r_abs = f_resolver.push(op_abs);
        auto r_ext_b = f_resolver.push(op_extract_b);
        auto r_ext_c = f_resolver.push(op_extract_c);
        auto r_id = f_resolver.push(op_identity);

        // sqrt(4.0) = 2.0
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_sqrt, [&](auto& n) {
                    n.child(r_term);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.size() == 8);
            TEST_ASSERT(stack.pop<float>() == 2.0f);
        }

        // neg(sqrt(4.0)) = -2.0
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_neg, [&](auto& n) {
                    n.child(r_sqrt, [&](auto& n2) {
                        n2.child(r_term);
                    });
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<float>() == -2.0f);
        }

        // abs(neg(4.0)) = 4.0
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_abs, [&](auto& n) {
                    n.child(r_neg, [&](auto& n2) {
                        n2.child(r_term);
                    });
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<float>() == 4.0f);
        }

        // sin(0.0) or cos(0.0), log(e)
        auto term_zero = []() -> float { return 0.0f; };
        auto term_e = []() -> float { return static_cast<float>(M_E); };
        auto r_zero = f_resolver.push(term_zero);
        auto r_e = f_resolver.push(term_e);

        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_log, [&](auto& n) {
                    n.child(r_e);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(std::abs(stack.pop<float>() - 1.0f) < 1e-5f);
        }

        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_cos, [&](auto& n) {
                    n.child(r_zero);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<float>() == 1.0f);
        }

        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_sin, [&](auto& n) {
                    n.child(r_zero);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<float>() == 0.0f);
        }

        // Struct projection
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_ext_b, [&](auto& n) {
                    n.child(r_pod);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<int32_t>() == 20);
        }

        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_id, [&](auto& n) {
                    n.child(r_ext_c, [&](auto& n2) {
                        n2.child(r_pod);
                    });
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<int32_t>() == 30);
        }
    }

    void test_homogeneous_multi_arg_and_ordering()
    {
        std::cout << "[Test] test_homogeneous_multi_arg_and_ordering..." << std::endl;

        type_resolver_t t_resolver;
        function_resolver_t f_resolver{t_resolver};

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
            auto tree = tree_builder_t{f_resolver}
                .root(r_sub, [&](auto& n) {
                    n.child(r_100);
                    n.child(r_20);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 80.0);
        }

        // Check div(100, 20) = 5.0 (not 0.2)
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_div, [&](auto& n) {
                    n.child(r_100);
                    n.child(r_20);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 5.0);
        }

        // Check order3(1, 2, 3) = 123
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_order3, [&](auto& n) {
                    n.child(r_1);
                    n.child(r_2);
                    n.child(r_3);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 123.0);
        }

        // Check order4(1, 2, 3, 4) = 1234
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_order4, [&](auto& n) {
                    n.child(r_1);
                    n.child(r_2);
                    n.child(r_3);
                    n.child(r_4);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 1234.0);
        }

        // Check order5(1, 2, 3, 4, 5) = 12345
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_order5, [&](auto& n) {
                    n.child(r_1);
                    n.child(r_2);
                    n.child(r_3);
                    n.child(r_4);
                    n.child(r_5);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 12345.0);
        }
    }

    void test_heterogeneous_multi_type()
    {
        std::cout << "[Test] test_heterogeneous_multi_type..." << std::endl;

        type_resolver_t t_resolver;
        function_resolver_t f_resolver{t_resolver};

        auto term_int = []() -> int { return 100; };
        auto term_float = []() -> float { return 25.5f; };
        auto term_double = []() -> double { return 500.25; };
        auto term_true = []() -> bool { return true; };
        auto term_false = []() -> bool { return false; };
        auto term_u8 = []() -> uint8_t { return 7; };
        auto term_u64 = []() -> uint64_t { return 1000000ULL; };
        auto term_i32 = []() -> int32_t { return -50; };

        auto term_pod1 = []() -> Pod1 { return Pod1{9}; };
        auto term_pod3 = []() -> Pod3 { return Pod3{1, 2, 3}; };
        auto term_pod7 = []() -> Pod7 { return Pod7{{10, 20, 30, 40, 50, 60, 70}}; };
        auto term_pod12 = []() -> Pod12 { return Pod12{10, 20, 30}; };
        auto term_pod24 = []() -> Pod24 { return Pod24{100, 200, 300}; };

        auto r_int = f_resolver.push(term_int);
        auto r_float = f_resolver.push(term_float);
        auto r_double = f_resolver.push(term_double);
        auto r_true = f_resolver.push(term_true);
        auto r_false = f_resolver.push(term_false);
        auto r_u8 = f_resolver.push(term_u8);
        auto r_u64 = f_resolver.push(term_u64);
        auto r_i32 = f_resolver.push(term_i32);
        auto r_pod1 = f_resolver.push(term_pod1);
        auto r_pod3 = f_resolver.push(term_pod3);
        auto r_pod7 = f_resolver.push(term_pod7);
        auto r_pod12 = f_resolver.push(term_pod12);
        auto r_pod24 = f_resolver.push(term_pod24);

        // Heterogeneous function 1: select (bool, float, float) -> float
        auto fn_select = [](bool cond, float a, float b) -> float {
            return cond ? a : b;
        };
        auto r_select = f_resolver.push(fn_select);

        // Heterogeneous function 2: mixed scalars (int, float, double, bool) -> double
        auto fn_scalars = [](int i, float f, double d, bool b) -> double {
            return b ? (i + f + d) : (i - f - d);
        };
        auto r_scalars = f_resolver.push(fn_scalars);

        // Heterogeneous function 3: PODs and scalars (Pod1, Pod12, double) -> double
        auto fn_pod_mix = [](Pod1 p1, Pod12 p12, double d) -> double {
            return p1.a + (p12.a + p12.b + p12.c) + d;
        };
        auto r_pod_mix = f_resolver.push(fn_pod_mix);

        // Heterogeneous function 4: irregular PODs (Pod3, Pod7, Pod24) -> uint64_t
        auto fn_irreg = [](Pod3 p3, Pod7 p7, Pod24 p24) -> uint64_t {
            uint64_t sum = p3.a + p3.b + p3.c;
            for (auto v : p7.data) sum += v;
            sum += p24.a + p24.b + p24.c;
            return sum;
        };
        auto r_irreg = f_resolver.push(fn_irreg);

        // Heterogeneous function 5: integer sizes (uint8_t, uint64_t, float, int32_t) -> double
        auto fn_ints = [](uint8_t u8, uint64_t u64, float f, int32_t i32) -> double {
            return static_cast<double>(u8) + static_cast<double>(u64) + static_cast<double>(f) + static_cast<double>(i32);
        };
        auto r_ints = f_resolver.push(fn_ints);

        // Test select with true -> 25.5
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_select, [&](auto& n) {
                    n.child(r_true);
                    n.child(r_float);
                    n.child(r_float);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<float>() == 25.5f);
        }

        // Test scalars with true: 100 + 25.5 + 500.25 = 625.75
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_scalars, [&](auto& n) {
                    n.child(r_int);
                    n.child(r_float);
                    n.child(r_double);
                    n.child(r_true);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 625.75);
        }

        // Test scalars with false: 100 - 25.5 - 500.25 = -425.75
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_scalars, [&](auto& n) {
                    n.child(r_int);
                    n.child(r_float);
                    n.child(r_double);
                    n.child(r_false);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == -425.75);
        }

        // Test pod mix: 9 + (10 + 20 + 30) + 500.25 = 569.25
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_pod_mix, [&](auto& n) {
                    n.child(r_pod1);
                    n.child(r_pod12);
                    n.child(r_double);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 569.25);
        }

        // Test irregular PODs: (1+2+3) + (10+20+30+40+50+60+70) + (100+200+300) = 6 + 280 + 600 = 886
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_irreg, [&](auto& n) {
                    n.child(r_pod3);
                    n.child(r_pod7);
                    n.child(r_pod24);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<uint64_t>() == 886ULL);
        }

        // Test mixed integer sizes: 7 + 1000000 + 25.5 + (-50) = 999982.5
        {
            auto tree = tree_builder_t{f_resolver}
                .root(r_ints, [&](auto& n) {
                    n.child(r_u8);
                    n.child(r_u64);
                    n.child(r_float);
                    n.child(r_i32);
                })
                .build();
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<double>() == 999982.5);
        }
    }

    void test_ephemeral_evaluation_stability()
    {
        std::cout << "[Test] test_ephemeral_evaluation_stability..." << std::endl;

        type_resolver_t t_resolver;
        function_resolver_t f_resolver{t_resolver};

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
        auto tree = tree_builder_t{f_resolver}
            .root(r_add, [&](auto& n) {
                n.ephemeral(r_ephem);
                n.ephemeral(r_ephem);
            })
            .build();

        TEST_ASSERT(call_count == 2);

        // Repeated evaluate calls MUST evaluate to 30.0 and NOT call generator again
        for (int i = 0; i < 20; ++i)
        {
            auto stack = tree.evaluate(f_resolver);
            TEST_ASSERT(stack.pop<float>() == 30.0f);
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
    test_heterogeneous_multi_type();
    test_ephemeral_evaluation_stability();

    std::cout << "============================================" << std::endl;
    std::cout << " All gp-tree-execution tests PASSED!        " << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
