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
#include "testing.h"

namespace
{
    using namespace blt::gp;

    void test_subtree_span_and_navigation()
    {
        std::cout << "[Test] test_subtree_span_and_navigation..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_a = []() -> float { return 1.0f; };
        auto term_b = []() -> float { return 2.0f; };
        auto term_c = []() -> float { return 3.0f; };
        auto ephem_d = []() -> float { return 4.0f; };

        auto op_neg = [](float x) -> float { return -x; };
        auto op_add = [](float x, float y) -> float { return x + y; };
        auto op_ternary = [](float x, float y, float z) -> float { return x + y + z; };

        auto r_a = f_resolver.push(term_a);
        auto r_b = f_resolver.push(term_b);
        auto r_c = f_resolver.push(term_c);
        auto r_d = f_resolver.push(ephem_d, true);
        auto r_neg = f_resolver.push(op_neg);
        auto r_add = f_resolver.push(op_add);
        auto r_ternary = f_resolver.push(op_ternary);

        // Build tree:
        // ternary(add(a, b), neg(c), d)
        // Operators in postorder:
        // [0: a, 1: b, 2: add, 3: c, 4: neg, 5: d, 6: ternary]
        auto tree = tree_builder_t<float>{f_resolver}
            .root(r_ternary, [&](auto& n) {
                n.child(r_add, [&](auto& n2) {
                    n2.child(r_a);
                    n2.child(r_b);
                });
                n.child(r_neg, [&](auto& n2) {
                    n2.child(r_c);
                });
                n.ephemeral(r_d);
            })
            .build();

        // 1. Root span
        auto root = tree.root();
        TEST_ASSERT(root.size() == 7);
        TEST_ASSERT(root.root_index() == 6);

        // 2. Subtree spans for each operator
        auto span_a = tree.subtree_span(0);
        TEST_ASSERT(span_a.size() == 1 && span_a.root_index() == 0);

        auto span_b = tree.subtree_span(1);
        TEST_ASSERT(span_b.size() == 1 && span_b.root_index() == 1);

        auto span_add = tree.subtree_span(2);
        TEST_ASSERT(span_add.size() == 3 && span_add.root_index() == 2);

        auto span_c = tree.subtree_span(3);
        TEST_ASSERT(span_c.size() == 1 && span_c.root_index() == 3);

        auto span_neg = tree.subtree_span(4);
        TEST_ASSERT(span_neg.size() == 2 && span_neg.root_index() == 4);

        auto span_d = tree.subtree_span(5);
        TEST_ASSERT(span_d.size() == 1 && span_d.root_index() == 5);

        auto span_root = tree.subtree_span(6);
        TEST_ASSERT(span_root == root);

        // Span equality and inequality
        TEST_ASSERT(span_a == span_a);
        TEST_ASSERT(span_a != span_b);

        // Invalid subtree span index
        bool threw = false;
        try
        {
            tree.subtree_span(100);
        }
        catch (const std::runtime_error&)
        {
            threw = true;
        }
        TEST_ASSERT(threw);

        // 3. Child navigation via child(index)
        // Children of root (ternary):
        // child(0) is rightmost child: 'd' (span [5, 6))
        // child(1) is middle child: 'neg(c)' (span [3, 5))
        // child(2) is leftmost child: 'add(a, b)' (span [0, 3))
        auto root_child0 = root.child(0);
        TEST_ASSERT(root_child0 == span_d);

        auto root_child1 = root.child(1);
        TEST_ASSERT(root_child1 == span_neg);

        auto root_child2 = root.child(2);
        TEST_ASSERT(root_child2 == span_add);

        // Subchild of add:
        // child(0) of add is rightmost child: 'b' (span [1, 2))
        // child(1) of add is leftmost child: 'a' (span [0, 1))
        TEST_ASSERT(root_child2.child(0) == span_b);
        TEST_ASSERT(root_child2.child(1) == span_a);

        // Subchild of neg:
        // child(0) of neg is 'c' (span [3, 4))
        TEST_ASSERT(root_child1.child(0) == span_c);
    }

    void test_children_iterator_semantics()
    {
        std::cout << "[Test] test_children_iterator_semantics..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_a = []() -> float { return 1.0f; };
        auto term_b = []() -> float { return 2.0f; };
        auto term_c = []() -> float { return 3.0f; };
        auto term_d = []() -> float { return 4.0f; };

        auto op_neg = [](float x) -> float { return -x; };
        auto op_add = [](float x, float y) -> float { return x + y; };
        auto op_ternary = [](float x, float y, float z) -> float { return x + y + z; };

        auto r_a = f_resolver.push(term_a);
        auto r_b = f_resolver.push(term_b);
        auto r_c = f_resolver.push(term_c);
        auto r_d = f_resolver.push(term_d);
        auto r_neg = f_resolver.push(op_neg);
        auto r_add = f_resolver.push(op_add);
        auto r_ternary = f_resolver.push(op_ternary);

        // Build tree: ternary(add(a, b), neg(c), d)
        auto tree = tree_builder_t<float>{f_resolver}
            .root(r_ternary, [&](auto& n) {
                n.child(r_add, [&](auto& n2) {
                    n2.child(r_a);
                    n2.child(r_b);
                });
                n.child(r_neg, [&](auto& n2) {
                    n2.child(r_c);
                });
                n.child(r_d);
            })
            .build();

        // 1. Iterate over children of root (3 children)
        auto root = tree.root();
        std::vector<tree_t<float>::subtree_span_t> root_children;
        for (auto child_span : root.children())
        {
            root_children.push_back(child_span);
        }

        TEST_ASSERT(root_children.size() == 3);
        TEST_ASSERT(root_children[0] == tree.subtree_span(5)); // d
        TEST_ASSERT(root_children[1] == tree.subtree_span(4)); // neg(c)
        TEST_ASSERT(root_children[2] == tree.subtree_span(2)); // add(a, b)

        // 2. Iterate over children of a leaf (0 children)
        auto leaf_span = tree.subtree_span(0);
        size_t leaf_child_count = 0;
        for (auto child : leaf_span.children())
        {
            (void)child;
            ++leaf_child_count;
        }
        TEST_ASSERT(leaf_child_count == 0);

        // 3. Test iterator operators: ++it, it++, *it, it->, ==, !=
        auto children = root.children();
        auto it1 = children.begin();
        auto it2 = children.begin();
        TEST_ASSERT(it1 == it2);

        TEST_ASSERT(it1->size() == 1); // d has size 1
        TEST_ASSERT((*it1).root_index() == 5);

        auto old_it = it1++;
        TEST_ASSERT(old_it == it2);
        TEST_ASSERT(it1 != it2);
        TEST_ASSERT(it1->size() == 2); // neg(c) has size 2

        ++it1;
        TEST_ASSERT(it1->size() == 3); // add(a, b) has size 3

        ++it1;
        TEST_ASSERT(it1 == children.end());
    }

    void test_to_tree_extraction()
    {
        std::cout << "[Test] test_to_tree_extraction..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_a = []() -> float { return 10.0f; };
        auto ephem_b = []() -> float { return 25.0f; };
        auto ephem_c = []() -> float { return 5.0f; };

        auto op_sub = [](float x, float y) -> float { return x - y; };
        auto op_mul = [](float x, float y) -> float { return x * y; };

        auto r_a = f_resolver.push(term_a);
        auto r_b = f_resolver.push(ephem_b, true);
        auto r_c = f_resolver.push(ephem_c, true);
        auto r_sub = f_resolver.push(op_sub);
        auto r_mul = f_resolver.push(op_mul);

        // Build tree: mul(sub(a, ephem_b), sub(ephem_c, a))
        // mul(10 - 25, 5 - 10) = -15 * -5 = 75
        auto tree = tree_builder_t<float>{f_resolver}
            .root(r_mul, [&](auto& n) {
                n.child(r_sub, [&](auto& n2) {
                    n2.child(r_a);
                    n2.ephemeral(r_b);
                });
                n.child(r_sub, [&](auto& n2) {
                    n2.ephemeral(r_c);
                    n2.child(r_a);
                });
            })
            .build();

        // Subtree 1: rightmost child of mul = sub(ephem_c, a) = 5 - 10 = -5
        auto child0 = tree.root().child(0);
        auto tree_sub1 = child0.to_tree();
        TEST_ASSERT(tree_sub1.root().size() == 3);
        auto res1 = tree_sub1.evaluate(f_resolver);
        TEST_ASSERT(res1 == -5.0f);

        // Subtree 2: leftmost child of mul = sub(a, ephem_b) = 10 - 25 = -15
        auto child1 = tree.root().child(1);
        auto tree_sub2 = child1.to_tree();
        TEST_ASSERT(tree_sub2.root().size() == 3);
        auto res2 = tree_sub2.evaluate(f_resolver);
        TEST_ASSERT(res2 == -15.0f);

        // Leaf extract: ephem_b
        auto leaf_b = child1.child(0); // in sub(a, ephem_b), child(0) is ephem_b
        auto tree_leaf_b = leaf_b.to_tree();
        TEST_ASSERT(tree_leaf_b.root().size() == 1);
        auto res_b = tree_leaf_b.evaluate(f_resolver);
        TEST_ASSERT(res_b == 25.0f);
    }

    void test_replace_and_erase_mutations()
    {
        std::cout << "[Test] test_replace_and_erase_mutations..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_a = []() -> float { return 100.0f; };
        auto term_b = []() -> float { return 20.0f; };
        auto ephem_e1 = []() -> float { return 7.0f; };
        auto ephem_e2 = []() -> float { return 3.0f; };

        auto op_add = [](float x, float y) -> float { return x + y; };
        auto op_sub = [](float x, float y) -> float { return x - y; };
        auto op_mul = [](float x, float y) -> float { return x * y; };
        auto op_neg = [](float x) -> float { return -x; };

        auto r_a = f_resolver.push(term_a);
        auto r_b = f_resolver.push(term_b);
        auto r_e1 = f_resolver.push(ephem_e1, true);
        auto r_e2 = f_resolver.push(ephem_e2, true);
        auto r_add = f_resolver.push(op_add);
        auto r_sub = f_resolver.push(op_sub);
        auto r_mul = f_resolver.push(op_mul);
        auto r_neg = f_resolver.push(op_neg);

        // Base tree: add(sub(a, ephem_e1), mul(ephem_e2, b))
        // add(100 - 7, 3 * 20) = 93 + 60 = 153
        auto make_base_tree = [&]() {
            return tree_builder_t<float>{f_resolver}
                .root(r_add, [&](auto& n) {
                    n.child(r_sub, [&](auto& n2) {
                        n2.child(r_a);
                        n2.ephemeral(r_e1);
                    });
                    n.child(r_mul, [&](auto& n2) {
                        n2.ephemeral(r_e2);
                        n2.child(r_b);
                    });
                })
                .build();
        };

        // 1. Replace standard leaf 'b' (20) with standard leaf 'a' (100)
        // in mul(ephem_e2, b), child(0) is 'b'
        {
            auto tree = make_base_tree();
            auto span_mul = tree.root().child(0);
            auto span_b = span_mul.child(0);

            auto leaf_a = tree_builder_t<float>{f_resolver}.root(r_a).build();
            tree.replace(span_b, leaf_a);

            // Now add(100 - 7, 3 * 100) = 93 + 300 = 393
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 393.0f);
        }

        // 2. Replace standard leaf 'b' (20) with ephemeral leaf 'e1' (7)
        {
            auto tree = make_base_tree();
            auto span_mul = tree.root().child(0);
            auto span_b = span_mul.child(0);

            auto leaf_e1 = tree_builder_t<float>{f_resolver}.root(r_add, [&](auto& n) {
                n.ephemeral(r_e1);
                n.child(r_a);
            }).build().root().child(1).to_tree(); // extract ephemeral e1

            tree.replace(span_b, leaf_e1);

            // Now add(100 - 7, 3 * 7) = 93 + 21 = 114
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 114.0f);
        }

        // 3. Replace ephemeral leaf 'e1' (7) with composite subtree 'neg(b)' (-20)
        {
            auto tree = make_base_tree();
            auto span_sub = tree.root().child(1);
            auto span_e1 = span_sub.child(0); // in sub(a, e1), child(0) is e1

            auto subtree_neg = tree_builder_t<float>{f_resolver}
                .root(r_neg, [&](auto& n) {
                    n.child(r_b);
                })
                .build();

            tree.replace(span_e1, subtree_neg);

            // Now add(100 - (-20), 3 * 20) = 120 + 60 = 180
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 180.0f);
        }

        // 4. Replace entire composite subtree 'mul(ephem_e2, b)' with standard leaf 'a' (100)
        {
            auto tree = make_base_tree();
            auto span_mul = tree.root().child(0);

            auto leaf_a = tree_builder_t<float>{f_resolver}.root(r_a).build();
            tree.replace(span_mul, leaf_a);

            // Now add(100 - 7, 100) = 93 + 100 = 193
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == 193.0f);
        }

        // 5. Replace entire root
        {
            auto tree = make_base_tree();
            auto new_tree = tree_builder_t<float>{f_resolver}
                .root(r_neg, [&](auto& n) {
                    n.ephemeral(r_e2);
                })
                .build();

            tree.replace(tree.root(), new_tree);
            auto res = tree.evaluate(f_resolver);
            TEST_ASSERT(res == -3.0f);
        }

        // 6. Subtree erasure via erase()
        {
            auto tree = make_base_tree();
            // Erase mul subtree
            auto span_mul = tree.root().child(0);
            tree.erase(span_mul);
            TEST_ASSERT(tree.root().size() == 4); // [a, e1, sub, add]
        }
    }

    void test_postorder_traversal()
    {
        std::cout << "[Test] test_postorder_traversal..." << std::endl;

        function_resolver_t<float> f_resolver;

        auto term_a = []() -> float { return 1.0f; };
        auto term_b = []() -> float { return 2.0f; };
        auto op_add = [](float x, float y) -> float { return x + y; };

        auto r_a = f_resolver.push(term_a);
        auto r_b = f_resolver.push(term_b);
        auto r_add = f_resolver.push(op_add);

        auto tree = tree_builder_t<float>{f_resolver}
            .root(r_add, [&](auto& n) {
                n.child(r_a);
                n.child(r_b);
            })
            .build();

        // postorder() iterates backward from root to first leaf
        size_t count = 0;
        std::vector<size_t> op_ids;
        for (const auto& op : tree.postorder())
        {
            op_ids.push_back(op.id.get());
            ++count;
        }

        TEST_ASSERT(count == 3);
        TEST_ASSERT(op_ids[0] == r_add.get());
        TEST_ASSERT(op_ids[1] == r_b.get());
        TEST_ASSERT(op_ids[2] == r_a.get());
    }
}

int main()
{
    std::cout << "============================================" << std::endl;
    std::cout << " Running gp-tree-mutation Unit Test Suite   " << std::endl;
    std::cout << "============================================" << std::endl;

    test_subtree_span_and_navigation();
    test_children_iterator_semantics();
    test_to_tree_extraction();
    test_replace_and_erase_mutations();
    test_postorder_traversal();

    std::cout << "============================================" << std::endl;
    std::cout << " All gp-tree-mutation tests PASSED!         " << std::endl;
    std::cout << "============================================" << std::endl;

    return 0;
}
