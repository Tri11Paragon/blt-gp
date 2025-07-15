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

#include <blt/gp/util/meta.h>
#include <blt/gp/typesystem.h>
#include <blt/gp/stack.h>
#include <blt/gp/fwdecl.h>
#include <blt/std/types.h>

#include <utility>
#include <stack>

namespace blt::gp
{
    // TODO: i feel like this should be in its own class
    struct operator_special_flags
    {
        explicit operator_special_flags(const bool is_ephemeral = false, const bool has_ephemeral_drop = false): m_ephemeral(is_ephemeral),
            m_ephemeral_drop(has_ephemeral_drop)
        {
        }

        [[nodiscard]] bool is_ephemeral() const
        {
            return m_ephemeral;
        }

        [[nodiscard]] bool has_ephemeral_drop() const
        {
            return m_ephemeral_drop;
        }

    private:
        bool m_ephemeral : 1;
        bool m_ephemeral_drop : 1;
    };

    static_assert(sizeof(operator_special_flags) == 1, "Size of operator flags struct is expected to be 1 byte!");

    struct op_container_t
    {
        op_container_t(const size_t type_size, const operator_id id, const bool is_value, const operator_special_flags flags):
            m_type_size(type_size), m_id(id), m_is_value(is_value), m_flags(flags)
        {
        }

        [[nodiscard]] auto type_size() const
        {
            return m_type_size;
        }

        [[nodiscard]] auto id() const
        {
            return m_id;
        }

        [[nodiscard]] auto is_value() const
        {
            return m_is_value;
        }

        [[nodiscard]] bool has_ephemeral_drop() const
        {
            return m_flags.has_ephemeral_drop();
        }

        [[nodiscard]] operator_special_flags get_flags() const
        {
            return m_flags;
        }

    private:
        size_t m_type_size;
        operator_id m_id;
        bool m_is_value;
        operator_special_flags m_flags;
    };

    class evaluation_context
    {
    public:
        explicit evaluation_context() = default;

        stack_allocator values;
    };

    inline size_t accumulate_type_sizes(const detail::op_iter_t begin, const detail::op_iter_t end)
    {
        size_t total = 0;
        for (auto it = begin; it != end; ++it)
        {
            if (it->is_value())
                total += it->type_size();
        }
        return total;
    }

    template <typename T>
    class evaluation_ref
    {
    public:
        explicit evaluation_ref(const bool ephemeral, T& value, evaluation_context& context): m_value(&value), m_context(&context)
        {
            if (ephemeral)
                m_value.bit(0, true);
        }

        evaluation_ref(const evaluation_ref& copy) = delete;
        evaluation_ref& operator=(const evaluation_ref& copy) = delete;

        evaluation_ref(evaluation_ref&& move) noexcept : m_value(move.m_value), m_context(move.m_context)
        {
            move.m_value = nullptr;
            move.m_context = nullptr;
        }

        evaluation_ref& operator=(evaluation_ref&& move) noexcept
        {
            m_value = std::exchange(m_value, move.m_value);
            m_context = std::exchange(m_context, move.m_context);
            return *this;
        }

        T& get()
        {
            return *m_value;
        }

        const T& get() const
        {
            return *m_value;
        }

        explicit operator T&()
        {
            return *m_value;
        }

        explicit operator T&() const
        {
            return *m_value;
        }

        T* operator->()
        {
            return m_value.get();
        }

        ~evaluation_ref()
        {
            if constexpr (detail::has_func_drop_v<detail::remove_cv_ref<T>>)
            {
                if (m_value.get() != nullptr)
                {
                    if (!m_value.bit(0))
                        m_value->drop();
                    m_context->values.reset();
                }
            }
        }

    private:
        mem::pointer_storage<T> m_value;
        evaluation_context* m_context;
    };

    class tree_t
    {
    public:
        struct child_t
        {
            ptrdiff_t start;
            // one past the end
            ptrdiff_t end;
        };

        struct subtree_point_t
        {
            ptrdiff_t pos;
            type_id type;

            explicit subtree_point_t(const ptrdiff_t pos): pos(pos), type(0)
            {
            }

            subtree_point_t(const ptrdiff_t pos, const type_id type): pos(pos), type(type)
            {
            }
        };

        struct byte_only_transaction_t
        {
            byte_only_transaction_t(tree_t& tree, const size_t bytes): tree(tree), data(nullptr), bytes(bytes)
            {
                move(bytes);
            }

            explicit byte_only_transaction_t(tree_t& tree): tree(tree), data(nullptr), bytes(0)
            {
            }

            byte_only_transaction_t(const byte_only_transaction_t& copy) = delete;
            byte_only_transaction_t& operator=(const byte_only_transaction_t& copy) = delete;

            byte_only_transaction_t(byte_only_transaction_t&& move) noexcept: tree(move.tree), data(std::exchange(move.data, nullptr)),
                                                                              bytes(std::exchange(move.bytes, 0))
            {
            }

            byte_only_transaction_t& operator=(byte_only_transaction_t&& move) noexcept = delete;

            void move(size_t bytes_to_move);

            [[nodiscard]] bool empty() const
            {
                return bytes == 0;
            }

            ~byte_only_transaction_t()
            {
                if (!empty())
                {
                    tree.values.copy_from(data, bytes);
                    bytes = 0;
                }
            }

        private:
            tree_t& tree;
            u8* data;
            size_t bytes;
        };

        explicit tree_t(gp_program& program): m_program(&program)
        {
        }

        tree_t(const tree_t& copy): m_program(copy.m_program)
        {
            copy_fast(copy);
        }

        tree_t& operator=(const tree_t& copy)
        {
            if (this == &copy)
                return *this;
            m_program = copy.m_program;
            copy_fast(copy);
            return *this;
        }

        /**
         * This function copies the data from the provided tree, will attempt to reserve and copy in one step.
         * will avoid reallocation if enough space is already present.
         *
         * This function is meant to copy into and replaces data inside the tree.
         */
        void copy_fast(const tree_t& copy)
        {
            if (this == &copy)
                return;

            operations.reserve(copy.operations.size());

            auto copy_it = copy.operations.begin();
            auto op_it = operations.begin();

            size_t total_op_bytes = 0;
            size_t total_copy_bytes = 0;

            for (; op_it != operations.end(); ++op_it)
            {
                if (copy_it == copy.operations.end())
                    break;
                if (copy_it->is_value())
                {
                    copy.handle_refcount_increment(copy_it, total_copy_bytes);
                    total_copy_bytes += copy_it->type_size();
                }
                if (op_it->is_value())
                {
                    handle_refcount_decrement(op_it, total_op_bytes);
                    total_op_bytes += op_it->type_size();
                }
                *op_it = *copy_it;
                ++copy_it;
            }
            const auto op_it_cpy = op_it;
            for (; op_it != operations.end(); ++op_it)
            {
                if (op_it->is_value())
                {
                    handle_refcount_decrement(op_it, total_op_bytes);
                    total_op_bytes += op_it->type_size();
                }
            }
            operations.erase(op_it_cpy, operations.end());
            for (; copy_it != copy.operations.end(); ++copy_it)
            {
                if (copy_it->is_value())
                {
                    copy.handle_refcount_increment(copy_it, total_copy_bytes);
                    total_copy_bytes += copy_it->type_size();
                }
                operations.emplace_back(*copy_it);
            }

            values.reserve(copy.values.stored());
            values.reset();
            values.insert(copy.values);
        }

        tree_t(tree_t&& move) = default;

        tree_t& operator=(tree_t&& move) = default;

        void clear(gp_program& program);

        void insert_operator(size_t index, const op_container_t& container);

        void insert_operator(const op_container_t& container)
        {
            operations.emplace_back(container);
            handle_operator_inserted(operations.back());
        }

        template <typename... Args>
        void emplace_operator(Args&&... args)
        {
            operations.emplace_back(std::forward<Args>(args)...);
            handle_operator_inserted(operations.back());
        }

        size_t get_depth(gp_program& program) const;

        /**
        *   Selects a random index inside this tree's operations stack
        *   @param terminal_chance if we select a terminal this is the chance we will actually pick it, otherwise continue the loop.
        */
        [[nodiscard]] subtree_point_t select_subtree(double terminal_chance = 0.1) const;
        /**
         *  Selects a random index inside the tree's operations stack, with a limit on the max number of times we will attempt to select this point.
         *  @param type type to find
         *  @param max_tries maximum number of times we are allowed to select a tree without finding a corresponding type.
         *  @param terminal_chance if we select a terminal this is the chance that we will actually pick it
         */
        [[nodiscard]] std::optional<subtree_point_t> select_subtree(type_id type, u32 max_tries = 5, double terminal_chance = 0.1) const;
        /**
        *   Select an index by traversing through the tree structure
        *   @param terminal_chance if we select a terminal this is the chance that we will actually pick it.
        *   @param depth_multiplier this controls how the depth contributes to the chance to exit.
        *       By default, a depth of 3.5 will have a 50% chance of returning the current index.
        */
        [[nodiscard]] subtree_point_t select_subtree_traverse(double terminal_chance = 0.1, double depth_multiplier = 0.6) const;
        /**
         *  SSelect an index by traversing through the tree structure, with a limit on the max number of times we will attempt to select this point.
         *  @param type type to find
         *  @param max_tries maximum number of times we are allowed to select a tree without finding a corresponding type.
         *  @param terminal_chance if we select a terminal this is the chance that we will actually pick it
         *  @param depth_multiplier this controls how the depth contributes to the chance to exit.
         *       By default, a depth of 3.5 will have a 50% chance of returning the current index.
         */
        [[nodiscard]] std::optional<subtree_point_t> select_subtree_traverse(type_id type, u32 max_tries = 5, double terminal_chance = 0.1,
                                                                             double depth_multiplier = 0.6) const;

        /**
                 * Copies the subtree found at point into the provided out params
                 * @param point subtree point
                 * @param extent how far the subtree extends
                 * @param operators vector for storing subtree operators
                 * @param stack stack for storing subtree values
                 */
        void copy_subtree(subtree_point_t point, ptrdiff_t extent, tracked_vector<op_container_t>& operators, stack_allocator& stack);

        /**
         * Copies the subtree found at point into the provided out params
         * @param point subtree point
         * @param operators vector for storing subtree operators
         * @param stack stack for storing subtree values
         */
        void copy_subtree(const subtree_point_t point, tracked_vector<op_container_t>& operators, stack_allocator& stack)
        {
            copy_subtree(point, find_endpoint(point.pos), operators, stack);
        }

        void copy_subtree(const subtree_point_t point, const ptrdiff_t extent, tree_t& out_tree)
        {
            copy_subtree(point, extent, out_tree.operations, out_tree.values);
        }

        void copy_subtree(const subtree_point_t point, tree_t& out_tree)
        {
            copy_subtree(point, find_endpoint(point.pos), out_tree);
        }

        /**
         * Swaps the subtrees between this tree and the other tree
         * @param our_subtree
         * @param other_tree
         * @param other_subtree
         */
        void swap_subtrees(subtree_point_t our_subtree, tree_t& other_tree, subtree_point_t other_subtree);

        /**
         * Replaces the point inside our tree with a new tree provided to this function.
         * Uses the extent instead of calculating it for removing the existing subtree
         * This can be used if you already have child tree information, such as when using @code find_child_extends@endcode
         * @param point point to replace at
         * @param extent extend of the subtree (child tree)
         * @param other_tree other tree to replace with
         */
        void replace_subtree(subtree_point_t point, ptrdiff_t extent, tree_t& other_tree);

        /**
         * Replaces the point inside our tree with a new tree provided to this function
         * @param point point to replace at
         * @param other_tree other tree to replace with
         */
        void replace_subtree(const subtree_point_t point, tree_t& other_tree)
        {
            replace_subtree(point, find_endpoint(point.pos), other_tree);
        }

        /**
         * Deletes the subtree at a point, bounded by extent. This is useful if you already know the size of the child tree
         * Note: if you provide an incorrectly sized extent this will create UB within the GP program
         * extent must be one past the last element in the subtree, as returned by all helper functions here.
         * @param point point to delete from
         * @param extent end point of the tree
         */
        void delete_subtree(subtree_point_t point, ptrdiff_t extent);

        /**
         * Deletes the subtree at a point
         * @param point point of subtree to recursively delete
         */
        void delete_subtree(const subtree_point_t point)
        {
            delete_subtree(point, find_endpoint(point.pos));
        }

        /**
         * Insert a subtree before the specified point
         * @param point point to insert into
         * @param other_tree the tree to insert
         * @return point + other_tree.size()
         */
        ptrdiff_t insert_subtree(subtree_point_t point, tree_t& other_tree);

        /**
         * temporarily moves the last bytes amount of data from the current stack. this can be useful for if you are going to do a lot
         * of consecutive operations on the tree as this will avoid extra copy + reinsert.
         * The object returned by this function will automatically move the data back in when it goes out of scope.
         * @param operator_index operator index to move from. this is inclusive
         */
        void temporary_move(const size_t)
        {
            // return obt_move_t{*this, operator_index};
        }

        void modify_operator(size_t point, operator_id new_id, std::optional<type_id> return_type = {});

        /**
        *   User function for evaluating this tree using a context reference. This function should only be used if the tree is expecting the context value
        *   This function returns a copy of your value, if it is too large for the stack, or you otherwise need a reference, please use the corresponding
        *   get_evaluation_ref function!
        */
        template <typename T, typename Context>
        T get_evaluation_value(const Context& context) const
        {
            auto& ctx = evaluate(context);
            auto val = ctx.values.template from<T>(0);
            evaluation_ref<T> ref{operations.front().get_flags().is_ephemeral(), val, ctx};
            return ref.get();
        }

        /**
        *  User function for evaluating this tree without a context reference. This function should only be used if the tree is expecting the context value
        *  This function returns a copy of your value, if it is too large for the stack, or you otherwise need a reference, please use the corresponding
        *  get_evaluation_ref function!
        */
        template <typename T>
        T get_evaluation_value() const
        {
            auto& ctx = evaluate();
            auto val = ctx.values.from<T>(0);
            evaluation_ref<T> ref{operations.front().get_flags().is_ephemeral(), val, ctx};
            return ref.get();
        }

        /**
        * User function for evaluating the tree with context returning a reference to the value.
        * The class returned is used to automatically drop the value when you are done using it
        */
        template <typename T, typename Context>
        evaluation_ref<T> get_evaluation_ref(const Context& context) const
        {
            auto& ctx = evaluate(context);
            auto& val = ctx.values.template from<T>(0);
            return evaluation_ref<T>{operations.front().get_flags().is_ephemeral(), val, ctx};
        }

        /**
        * User function for evaluating the tree without context returning a reference to the value.
        * The class returned is used to automatically drop the value when you are done using it
        */
        template <typename T>
        evaluation_ref<T> get_evaluation_ref() const
        {
            auto& ctx = evaluate();
            auto& val = ctx.values.from<T>(0);
            return evaluation_ref<T>{operations.front().get_flags().is_ephemeral(), val, ctx};
        }

        void print(std::ostream& out, bool print_literals = true, bool pretty_indent = false, bool include_types = false,
                   ptrdiff_t marked_index = -1) const;

        bool check(void* context) const;

        void find_child_extends(tracked_vector<child_t>& vec, blt::size_t parent_node, blt::size_t argc) const;

        // places one past the end of the child. so it's [start, end)
        [[nodiscard]] ptrdiff_t find_endpoint(blt::ptrdiff_t start) const;

        // valid for [begin, end)
        static size_t total_value_bytes(const detail::const_op_iter_t begin, const detail::const_op_iter_t end)
        {
            size_t total = 0;
            for (auto it = begin; it != end; ++it)
            {
                if (it->is_value())
                    total += it->type_size();
            }
            return total;
        }

        [[nodiscard]] size_t total_value_bytes(const size_t begin, const size_t end) const
        {
            return total_value_bytes(operations.begin() + static_cast<ptrdiff_t>(begin),
                                     operations.begin() + static_cast<ptrdiff_t>(end));
        }

        [[nodiscard]] size_t total_value_bytes(const size_t begin) const
        {
            return total_value_bytes(operations.begin() + static_cast<ptrdiff_t>(begin), operations.end());
        }

        [[nodiscard]] size_t total_value_bytes() const
        {
            return total_value_bytes(operations.begin(), operations.end());
        }

        [[nodiscard]] size_t size() const
        {
            return operations.size();
        }

        [[nodiscard]] const op_container_t& get_operator(const size_t point) const
        {
            return operations[point];
        }

        [[nodiscard]] subtree_point_t subtree_from_point(ptrdiff_t point) const;

        template <typename Context, typename... Operators>
        static auto make_execution_lambda(size_t call_reserve_size, Operators&... operators)
        {
            return [call_reserve_size, &operators...](const tree_t& tree, void* context) -> evaluation_context& {
                const auto& ops = tree.operations;
                const auto& vals = tree.values;

                thread_local evaluation_context results{};
                results.values.reset();
                results.values.reserve(call_reserve_size);

                size_t total_so_far = 0;

                for (const auto& operation : iterate(ops).rev())
                {
                    if (operation.is_value())
                    {
                        total_so_far += operation.type_size();
                        results.values.copy_from(vals.from(total_so_far), operation.type_size());
                        continue;
                    }
                    call_jmp_table<Context>(operation.id(), context, results.values, results.values, operators...);
                }

                return results;
            };
        }

        ~tree_t()
        {
            clear(*m_program);
        }

        static tree_t& get_thread_local(gp_program& program);

    private:
        void handle_operator_inserted(const op_container_t& op);

        void handle_ptr_empty(const mem::pointer_storage<std::atomic_uint64_t>& ptr, u8* data, operator_id id) const;

        template <typename Iter>
        void handle_refcount_decrement(const Iter iter, const size_t forward_bytes) const
        {
            if (iter->get_flags().is_ephemeral() && iter->has_ephemeral_drop())
            {
                auto [val, ptr] = values.access_pointer_forward(forward_bytes, iter->type_size());
                --*ptr;
                if (*ptr == 0)
                    handle_ptr_empty(ptr, val, iter->id());
            }
        }

        template <typename Iter>
        void handle_refcount_increment(const Iter iter, const size_t forward_bytes) const
        {
            if (iter->get_flags().is_ephemeral() && iter->has_ephemeral_drop())
            {
                auto [_, ptr] = values.access_pointer_forward(forward_bytes, iter->type_size());
                ++*ptr;
            }
        }

        template <typename T, std::enable_if_t<!(std::is_pointer_v<T> || std::is_null_pointer_v<T>), bool>  = true>
        [[nodiscard]] evaluation_context& evaluate(const T& context) const
        {
            return evaluate(const_cast<void*>(static_cast<const void*>(&context)));
        }

        [[nodiscard]] evaluation_context& evaluate() const
        {
            return evaluate(nullptr);
        }

        [[nodiscard]] evaluation_context& evaluate(void* ptr) const;

        tracked_vector<op_container_t> operations;
        stack_allocator values;
        gp_program* m_program;

        /*
         * Static members
         * --------------
         */
    protected:
        template <typename Context, typename Operator>
        static void execute(void* context, stack_allocator& write_stack, stack_allocator& read_stack, Operator& operation)
        {
            if constexpr (std::is_same_v<detail::remove_cv_ref<typename Operator::First_Arg>, Context>)
            {
                write_stack.push(operation(context, read_stack));
            }
            else
            {
                write_stack.push(operation(read_stack));
            }
        }

        template <typename Context, size_t id, typename Operator>
        static bool call(const size_t op, void* context, stack_allocator& write_stack, stack_allocator& read_stack, Operator& operation)
        {
            if (id == op)
            {
                execute<Context>(context, write_stack, read_stack, operation);
                return false;
            }
            return true;
        }

        template <typename Context, typename... Operators, size_t... operator_ids>
        static void call_jmp_table_internal(size_t op, void* context, stack_allocator& write_stack, stack_allocator& read_stack,
                                            std::integer_sequence<size_t, operator_ids...>, Operators&... operators)
        {
            if (op >= sizeof...(operator_ids))
            {
                BLT_UNREACHABLE;
            }
            (call<Context, operator_ids>(op, context, write_stack, read_stack, operators) && ...);
        }

        template <typename Context, typename... Operators>
        static void call_jmp_table(size_t op, void* context, stack_allocator& write_stack, stack_allocator& read_stack,
                                   Operators&... operators)
        {
            call_jmp_table_internal<Context>(op, context, write_stack, read_stack, std::index_sequence_for<Operators...>(), operators...);
        }
    };

    struct fitness_t
    {
        double raw_fitness = 0;
        double standardized_fitness = 0;
        double adjusted_fitness = 0;
        i64 hits = 0;

        /**
         * Sets fitness such that larger values of raw_fitness are worse
         */
        void set_normal(const double raw_fit)
        {
            raw_fitness = raw_fit;
            standardized_fitness = raw_fit;
            adjusted_fitness = 1 / (1 + raw_fit);
        }

        /**
         * Sets fitness such that smaller values of raw_fitness are worse
         * Note: this function loses accuracy the larger the number. if your desired fitness is around 0-20 (raw) then this will work
         */
        void set_inverted_normal(const double raw_fit)
        {
            raw_fitness = raw_fit;
            standardized_fitness = raw_fit;
            adjusted_fitness = 1 - (1 / (1 + raw_fit));
        }
    };

    struct individual_t
    {
        tree_t tree;
        fitness_t fitness;

        void copy_fast(const tree_t& copy)
        {
            // fast copy of the tree
            tree.copy_fast(copy);
            // reset fitness
            fitness = {};
        }

        individual_t() = delete;

        explicit individual_t(tree_t&& tree): tree(std::move(tree))
        {
        }

        explicit individual_t(const tree_t& tree): tree(tree)
        {
        }

        individual_t(const individual_t&) = default;

        individual_t(individual_t&&) = default;

        individual_t& operator=(const individual_t&) = delete;

        individual_t& operator=(individual_t&&) = default;
    };

    class population_t
    {
    public:
        class population_tree_iterator
        {
        public:
            population_tree_iterator(tracked_vector<individual_t>& ind, blt::size_t pos): ind(ind), pos(pos)
            {
            }

            auto begin()
            {
                return population_tree_iterator(ind, 0);
            }

            auto end()
            {
                return population_tree_iterator(ind, ind.size());
            }

            population_tree_iterator operator++(int)
            {
                auto prev = pos++;
                return {ind, prev};
            }

            population_tree_iterator operator++()
            {
                return {ind, ++pos};
            }

            tree_t& operator*() const
            {
                return ind[pos].tree;
            }

            tree_t& operator->() const
            {
                return ind[pos].tree;
            }

            friend bool operator==(population_tree_iterator a, population_tree_iterator b)
            {
                return a.pos == b.pos;
            }

            friend bool operator!=(population_tree_iterator a, population_tree_iterator b)
            {
                return a.pos != b.pos;
            }

        private:
            tracked_vector<individual_t>& ind;
            blt::size_t pos;
        };

        tracked_vector<individual_t>& get_individuals()
        {
            return individuals;
        }

        [[nodiscard]] const tracked_vector<individual_t>& get_individuals() const
        {
            return individuals;
        }

        population_tree_iterator for_each_tree()
        {
            return population_tree_iterator{individuals, 0};
        }

        auto begin()
        {
            return individuals.begin();
        }

        auto end()
        {
            return individuals.end();
        }

        [[nodiscard]] auto begin() const
        {
            return individuals.begin();
        }

        [[nodiscard]] auto end() const
        {
            return individuals.end();
        }

        void clear()
        {
            individuals.clear();
        }

        population_t() = default;

        population_t(const population_t&) = default;

        population_t(population_t&&) = default;

        population_t& operator=(const population_t&) = delete;

        population_t& operator=(population_t&&) = default;

    private:
        tracked_vector<individual_t> individuals;
    };
}

#endif //BLT_GP_TREE_H
