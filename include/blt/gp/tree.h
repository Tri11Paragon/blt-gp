#pragma once
/*
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

#ifndef BLT_GP_TREE_H
#define BLT_GP_TREE_H

#include <algorithm>
#include <blt/std/hashmap.h>
#include "functional.h"

#include "blt/logging/logging.h"
#include "blt/std/assert.h"
#include "blt/std/types.h"

namespace blt::gp
{
    /**
     * Used for user defined manually generated trees as examples in code
     */
    template <typename Type, template<typename> typename Alloc = std::allocator>
    class tree_builder_t;

    /**
     * Used for generating trees in code by algorithms
     */
    template <typename Type, template<typename> typename Alloc = std::allocator>
    class generative_tree_builder_t;

    template <typename Type, template<typename> typename Alloc = std::allocator>
    struct tree_t
    {
        struct subtree_span_t
        {
            subtree_span_t() = default;

            subtree_span_t(const tree_t* tree, const size_t begin, const size_t end) : tree(tree), begin(begin), end(end)
            {
            }

            [[nodiscard]] size_t size() const
            {
                return end - begin;
            }

            [[nodiscard]] size_t root_index() const
            {
                return end - 1;
            }

            /**
             * Return the subtree span corresponding to this tree's `index` child.
             */
            [[nodiscard]] subtree_span_t child(size_t index) const
            {
                auto child_index = root_index() - 1;
                while (index--)
                {
                    auto subchild_span = tree->subtree_span(child_index);
                    child_index = subchild_span.begin - 1;
                }
                return tree->subtree_span(child_index);
            }

            [[nodiscard]] auto children() const
            {
                return children_t{*this};
            }

            /**
             * Returns a copy of the underlying subtree
             */
            tree_t to_tree()
            {
                const size_t prev_ephem = tree->leading_ephemeral_size(begin);
                size_t end_ephem = prev_ephem;
                // copy operators and make use of known ephemeral sizes to copy only the subtree's ephemeral values
                tree_t subtree;
                subtree.operators.reserve(size());
                for (auto it = tree->operators.begin() + begin; it != tree->operators.begin() + end; ++it)
                {
                    subtree.operators.push_back(*it);
                    if (it->stores_in_stack)
                        end_ephem += 1;
                }
                subtree.ephemeral_stack.insert(subtree.ephemeral_stack.end(), tree->ephemeral_stack.begin() + prev_ephem,
                                               tree->ephemeral_stack.begin() + end_ephem);
                return subtree;
            }

            Type evaluate(function_resolver_t<Type, Alloc>& resolver)
            {
                std::vector<Type, Alloc<Type>> stack;
                size_t ephemeral_offset = tree->leading_ephemeral_size(begin);
                for (auto it = tree->operators.begin() + begin; it != tree->operators.begin() + end; ++it)
                {
                    auto& op = *it;
                    if (op.stores_in_stack)
                    {
                        stack.push_back(tree->ephemeral_stack[ephemeral_offset]);
                        ephemeral_offset += 1;
                    }
                    else
                    {
                        resolver.forward(op.id, stack, stack);
                    }
                }
                return stack.back();
            }

            friend bool operator==(const subtree_span_t& a, const subtree_span_t& b)
            {
                return a.tree == b.tree && a.begin == b.begin && a.end == b.end;
            }

            friend bool operator!=(const subtree_span_t& a, const subtree_span_t& b)
            {
                return !(a == b);
            }

            struct children_iter_t
            {
                using iterator_category = std::forward_iterator_tag;
                using value_type = subtree_span_t;
                using difference_type = std::ptrdiff_t;
                using pointer = const subtree_span_t*;
                using reference = const subtree_span_t&;

                explicit children_iter_t(const subtree_span_t parent, const bool is_end = false)
                    : parent(parent)
                {
                    max_child = parent.tree->operators[parent.root_index()].argument_count;

                    if (is_end || max_child == 0)
                    {
                        child = max_child;
                        current = subtree_span_t{parent.tree, parent.begin, parent.begin};
                        return;
                    }

                    child = 0;
                    current = parent.tree->subtree_span(parent.root_index() - 1);
                }

                reference operator*() const
                {
                    return current;
                }

                pointer operator->() const
                {
                    return &current;
                }

                children_iter_t& operator++()
                {
                    ++child;

                    if (child >= max_child)
                        return *this;

                    current = parent.tree->subtree_span(current.begin - 1);
                    return *this;
                }

                children_iter_t operator++(int)
                {
                    auto old = *this;
                    ++(*this);
                    return old;
                }

                bool operator==(const children_iter_t& other) const
                {
                    return parent == other.parent && child == other.child;
                }

                bool operator!=(const children_iter_t& other) const
                {
                    return !(*this == other);
                }

                size_t child = 0;
                size_t max_child = 0;
                subtree_span_t parent;
                subtree_span_t current;
            };

            struct children_t
            {
                explicit children_t(const subtree_span_t span) : span(span)
                {
                }

                [[nodiscard]] children_iter_t begin() const
                {
                    return children_iter_t{span, false};
                }

                [[nodiscard]] children_iter_t end() const
                {
                    return children_iter_t{span, true};
                }

                subtree_span_t span;
            };

        private:
            friend tree_t;
            const tree_t* tree = nullptr;
            size_t begin = 0;
            size_t end = 0;
        };

        template <typename T>
        struct postorder_t
        {
            explicit postorder_t(T& tree) : tree(&tree)
            {
            }

            auto begin() const
            {
                return tree->operators.rbegin();
            }

            auto end() const
            {
                return tree->operators.rend();
            }

            auto rbegin() const
            {
                return tree->operators.rbegin();
            }

            auto rend() const
            {
                return tree->operators.rend();
            }

        private:
            T* tree;
        };

        explicit tree_t() = default;

        Type evaluate(function_resolver_t<Type, Alloc>& resolver)
        {
            return root().evaluate(resolver);
        }

        [[nodiscard]] subtree_span_t root() const
        {
            return subtree_span_t{this, 0, operators.size()};
        }

        [[nodiscard]] subtree_span_t subtree_span(const size_t root_operator_index) const
        {
            size_t needed = 1;

            for (size_t i = root_operator_index + 1; i-- > 0;)
            {
                if (i >= operators.size())
                    break;
                --needed;
                needed += operators[i].argument_count;

                if (needed == 0)
                    return subtree_span_t{this, i, root_operator_index + 1};
            }
            throw std::runtime_error{"invalid operator index, unable to find subtree's end point."};
        }

        /**
         * Replaces this subtree with the given replacement subtree.
         * This invalidates ALL spans after the beginning of this subtree.
         */
        void replace(const subtree_span_t subtree, const tree_t& replacement)
        {
            BLT_ASSERT(subtree.tree == this);
            BLT_ASSERT(subtree.begin < operators.size());
            BLT_ASSERT(subtree.end <= operators.size());

            auto before_bytes = leading_ephemeral_size(subtree.begin);
            auto after_bytes = trailing_ephemeral_size(subtree.end);
            ephemeral_stack.erase(ephemeral_stack.begin() + before_bytes, ephemeral_stack.begin() + ephemeral_stack.size() - after_bytes);
            ephemeral_stack.insert(ephemeral_stack.begin() + before_bytes, replacement.ephemeral_stack.begin(), replacement.ephemeral_stack.end());
            operators.erase(operators.begin() + subtree.begin, operators.begin() + subtree.end);
            operators.insert(operators.begin() + subtree.begin, replacement.operators.begin(), replacement.operators.end());
        }

        void erase(const subtree_span_t subtree)
        {
            BLT_ASSERT(subtree.tree == this);
            BLT_ASSERT(subtree.begin < operators.size());
            BLT_ASSERT(subtree.end <= operators.size());

            auto before_bytes = leading_ephemeral_size(subtree.begin);
            auto after_bytes = trailing_ephemeral_size(subtree.end);
            ephemeral_stack.erase(ephemeral_stack.begin() + before_bytes, ephemeral_stack.begin() + ephemeral_stack.size() - after_bytes);
            operators.erase(operators.begin() + subtree.begin, operators.begin() + subtree.end);
        }

        [[nodiscard]] auto postorder()
        {
            return postorder_t{*this};
        }

        [[nodiscard]] auto postorder() const
        {
            return postorder_t{*this};
        }

    protected:
        /**
         * Returns the number of ephemeral values before the start of the subtree according to standard order.
         * @param end_index: the index corresponding to the start of the subtree. Not included in the calculation.
         */
        [[nodiscard]] size_t leading_ephemeral_size(size_t end_index) const
        {
            size_t prev_ephem = 0;
            for (auto it = operators.begin(); it != operators.begin() + end_index; ++it)
            {
                if (it->stores_in_stack)
                    prev_ephem += 1;
            }
            return prev_ephem;
        }

        /**
         * Returns the number of ephemeral values after the end of the subtree according to standard order.
         * @param begin_index: The index one past the last operator in the subtree. Is included as part of the calculation.
         */
        [[nodiscard]] size_t trailing_ephemeral_size(size_t begin_index) const
        {
            size_t post_ephem = 0;
            for (auto it = operators.begin() + begin_index; it != operators.end(); ++it)
            {
                if (it->stores_in_stack)
                    post_ephem += 1;
            }
            return post_ephem;
        }

        friend postorder_t<tree_t>;
        friend postorder_t<tree_t const>;
        friend tree_builder_t<Type, Alloc>;
        friend generative_tree_builder_t<Type, Alloc>;
        friend subtree_span_t;
        // operators are stored in post order
        std::vector<function_info_t, Alloc<function_info_t>> operators;
        std::vector<Type> ephemeral_stack;
    };

    template <typename Type, template<typename> typename Alloc>
    class tree_builder_t
    {
    public:
        explicit tree_builder_t(function_resolver_t<Type, Alloc>& resolver) : resolver(&resolver)
        {
        }

        tree_builder_t& root(const function_id_t id)
        {
            emit(id);
            has_root = true;
            return *this;
        }

        template <typename Children>
        tree_builder_t& root(const function_id_t id, Children&& children)
        {
            node_builder_t node{*this};
            children(node);
            emit(id);
            has_root = true;
            return *this;
        }

        tree_t<Type, Alloc> build()
        {
            return std::move(tree);
        }

    private:
        class node_builder_t
        {
        public:
            explicit node_builder_t(tree_builder_t& parent) : parent(parent)
            {
            }

            node_builder_t& child(const function_id_t id)
            {
                if (!parent.resolver->get(id).is_terminal())
                {
                    throw std::runtime_error(string::format("Function with id: {} is not a terminal and requires {} arguments", id.get(),
                                                            parent.resolver->get(id).argument_count));
                }
                if (parent.resolver->stores_in_stack(id))
                {
                    return ephemeral(id);
                }
                ++child_count;
                parent.emit(id);
                return *this;
            }

            template <typename Children>
            node_builder_t& child(const function_id_t id, Children&& children)
            {
                node_builder_t node{parent};
                children(node);

                const auto expected = parent.resolver->get(id).argument_count;
                const auto actual = node.child_count;

                if (expected != actual)
                {
                    throw std::runtime_error(string::format(
                        "Number of children '{}' does not match expected number of arguments '{}' for function with id: {}", node.child_count,
                        expected, id.get()));
                }

                ++child_count;
                parent.emit(id);

                return *this;
            }

            node_builder_t& ephemeral(const function_id_t id)
            {
                ++child_count;
                parent.emit_ephemeral(id);
                return *this;
            }

        private:
            size_t child_count = 0;
            tree_builder_t& parent;
        };

        void emit(const function_id_t id)
        {
            tree.operators.push_back(resolver->get(id));
        }

        void emit_ephemeral(const function_id_t id)
        {
            resolver->forward(id, tree.ephemeral_stack, tree.ephemeral_stack);
            tree.operators.push_back(resolver->get(id));
        }

    private:
        function_resolver_t<Type, Alloc>* resolver;
        tree_t<Type, Alloc> tree;
        bool has_root = false;
    };

    template <typename Type, template<typename> typename Alloc>
    class generative_tree_builder_t
    {
    public:
        explicit generative_tree_builder_t(function_resolver_t<Type, Alloc>& resolver) : resolver(&resolver)
        {
        }

        generative_tree_builder_t& begin(const function_id_t id)
        {
            const auto& info = resolver->get(id);

            if (info.is_terminal())
            {
                throw std::runtime_error(string::format(
                    "Cannot begin terminal function with id: {}. Use leaf() or ephemeral() instead.", id.get()));
            }

            if (root_emitted && pending.empty())
            {
                throw std::runtime_error{"Cannot begin another root after the tree root has already been completed."};
            }

            pending.push_back(pending_node_t{id});
            return *this;
        }

        generative_tree_builder_t& leaf(const function_id_t id)
        {
            const auto& info = resolver->get(id);

            if (!info.is_terminal())
            {
                throw std::runtime_error(string::format(
                    "Function with id: {} is not a terminal and requires {} arguments.", id.get(), info.argument_count));
            }

            if (resolver->stores_in_stack(id))
                return ephemeral(id);

            emit(id);
            count_child();

            return *this;
        }

        generative_tree_builder_t& ephemeral(const function_id_t id)
        {
            const auto& info = resolver->get(id);

            if (!info.is_terminal())
            {
                throw std::runtime_error(string::format(
                    "Ephemeral function with id: {} must be terminal, but requires {} arguments.", id.get(), info.argument_count));
            }

            if (!resolver->stores_in_stack(id))
            {
                throw std::runtime_error(string::format(
                    "Function with id: {} is not registered as an ephemeral stack value.", id.get()));
            }

            emit_ephemeral(id);
            count_child();

            return *this;
        }

        generative_tree_builder_t& end()
        {
            if (pending.empty())
                throw std::runtime_error{"Cannot end node because there are no pending nodes."};

            auto node = pending.back();
            pending.pop_back();

            const auto& info = resolver->get(node.id);
            const auto expected = info.argument_count;

            if (node.emitted_children != expected)
            {
                throw std::runtime_error(string::format(
                    "Cannot close function with id: {}. Expected {} children, but got {}.",
                    node.id.get(), expected, node.emitted_children));
            }

            emit(node.id);

            if (pending.empty())
            {
                if (root_emitted)
                    throw std::runtime_error{"Cannot emit more than one root node."};

                root_emitted = true;
            }
            else
            {
                count_child();
            }

            return *this;
        }

        tree_t<Type, Alloc> build()
        {
            if (!pending.empty())
            {
                throw std::runtime_error(string::format(
                    "Cannot build tree because {} node(s) are still pending.", pending.size()));
            }

            if (!root_emitted)
                throw std::runtime_error{"Cannot build tree because no root node has been emitted."};

            return std::move(tree);
        }

        [[nodiscard]] bool empty() const
        {
            return tree.operators.empty() && pending.empty() && !root_emitted;
        }

        [[nodiscard]] size_t pending_count() const
        {
            return pending.size();
        }

    private:
        struct pending_node_t
        {
            explicit pending_node_t(const function_id_t id) : id(id)
            {
            }

            function_id_t id;
            size_t emitted_children = 0;
        };

        void count_child()
        {
            if (pending.empty())
            {
                if (root_emitted)
                    throw std::runtime_error{"Cannot emit another root node."};

                root_emitted = true;
                return;
            }

            auto& parent = pending.back();
            ++parent.emitted_children;

            const auto expected = resolver->get(parent.id).argument_count;

            if (parent.emitted_children > expected)
            {
                throw std::runtime_error(string::format(
                    "Too many children emitted for function with id: {}. Expected {}, but got at least {}.",
                    parent.id.get(), expected, parent.emitted_children));
            }
        }

        void emit(const function_id_t id)
        {
            tree.operators.push_back(resolver->get(id));
        }

        void emit_ephemeral(const function_id_t id)
        {
            resolver->forward(id, tree.ephemeral_stack, tree.ephemeral_stack);
            tree.operators.push_back(resolver->get(id));
        }

    private:
        function_resolver_t<Type, Alloc>* resolver;
        tree_t<Type, Alloc> tree;
        std::vector<pending_node_t, Alloc<pending_node_t>> pending;
        bool root_emitted = false;
    };

    extern template struct tree_t<float>;
    extern template struct tree_t<double>;
    extern template struct tree_t<i8>;
    extern template struct tree_t<u8>;
    extern template struct tree_t<u32>;
    extern template struct tree_t<i32>;
    extern template struct tree_t<i64>;
    extern template struct tree_t<u64>;

    // TODO: layer based approach.
}

#endif //BLT_GP_TREE_H
