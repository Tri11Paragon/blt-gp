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
#include <ostream>
#include <atomic>
#include <bits/locale_facets_nonio.h>

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

    template <typename T>
    class evaluation_ref
    {
    public:
        explicit evaluation_ref(T& value, evaluation_context& context): m_value(&value), m_context(&context)
        {
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

        ~evaluation_ref()
        {
            if constexpr (detail::has_func_drop_v<T>)
            {
                if (m_value != nullptr)
                {
                    m_value->drop();
                    m_context->values.reset();
                }
            }
        }

    private:
        T* m_value;
        evaluation_context* m_context;
    };

    class tree_t
    {
    public:
        explicit tree_t(gp_program& program);

        tree_t(const tree_t& copy): func(copy.func)
        {
            copy_fast(copy);
        }

        tree_t& operator=(const tree_t& copy)
        {
            if (this == &copy)
                return *this;
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

            for (; op_it != operations.end(); ++op_it)
            {
                if (op_it->has_ephemeral_drop())
                {
                }
                if (copy_it == copy.operations.end())
                    break;
                *op_it = *copy_it;
                if (copy_it->has_ephemeral_drop())
                {
                }
                ++copy_it;
            }
            const auto op_it_cpy = op_it;
            for (; op_it != operations.end(); ++op_it)
            {
                if (op_it->has_ephemeral_drop())
                {
                }
            }
            operations.erase(op_it_cpy, operations.end());
            for (; copy_it != copy.operations.end(); ++copy_it)
                operations.emplace_back(*copy_it);

            values.reserve(copy.values.stored());
            values.reset();
            values.insert(copy.values);
        }

        tree_t(tree_t&& move) = default;

        tree_t& operator=(tree_t&& move) = default;

        void clear(gp_program& program);

        struct child_t
        {
            ptrdiff_t start;
            // one past the end
            ptrdiff_t end;
        };

        void insert_operator(const op_container_t& container)
        {
            operations.emplace_back(container);
        }

        template <typename... Args>
        void emplace_operator(Args&&... args)
        {
            operations.emplace_back(std::forward<Args>(args)...);
        }

        [[nodiscard]] tracked_vector<op_container_t>& get_operations()
        {
            return operations;
        }

        [[nodiscard]] const tracked_vector<op_container_t>& get_operations() const
        {
            return operations;
        }

        [[nodiscard]] stack_allocator& get_values()
        {
            return values;
        }

        [[nodiscard]] const stack_allocator& get_values() const
        {
            return values;
        }

        blt::size_t get_depth(gp_program& program) const;


        /**
        *  User function for evaluating this tree using a context reference. This function should only be used if the tree is expecting the context value
        *  This function returns a copy of your value, if it is too large for the stack, or you otherwise need a reference, please use the corresponding
        *  get_evaluation_ref function!
        */
        template <typename T, typename Context>
        T get_evaluation_value(const Context& context) const
        {
            auto& ctx = evaluate(context);
            auto val = ctx.values.template from<T>(0);
            if constexpr (detail::has_func_drop_v<T>)
            {
                ctx.values.template from<T>(0).drop();
            }
            ctx.values.reset();
            return val;
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
            if constexpr (detail::has_func_drop_v<T>)
            {
                ctx.values.from<T>(0).drop();
            }
            ctx.values.reset();
            return val;
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
            return evaluation_ref<T>{val, ctx};
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
            return evaluation_ref<T>{val, ctx};
        }

        void print(gp_program& program, std::ostream& output, bool print_literals = true, bool pretty_indent = false,
                   bool include_types = false) const;

        bool check(gp_program& program, void* context) const;

        void find_child_extends(gp_program& program, tracked_vector<child_t>& vec, blt::size_t parent_node, blt::size_t argc) const;

        // places one past the end of the child. so it's [start, end)
        blt::ptrdiff_t find_endpoint(blt::gp::gp_program& program, blt::ptrdiff_t start) const;

        blt::ptrdiff_t find_parent(blt::gp::gp_program& program, blt::ptrdiff_t start) const;

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
            return total_value_bytes(operations.begin() + static_cast<blt::ptrdiff_t>(begin),
                                     operations.begin() + static_cast<blt::ptrdiff_t>(end));
        }

        [[nodiscard]] size_t total_value_bytes(const size_t begin) const
        {
            return total_value_bytes(operations.begin() + static_cast<blt::ptrdiff_t>(begin), operations.end());
        }

        [[nodiscard]] size_t total_value_bytes() const
        {
            return total_value_bytes(operations.begin(), operations.end());
        }

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

    private:
        template <typename T, std::enable_if_t<!(std::is_pointer_v<T> || std::is_null_pointer_v<T>), bool>  = true>
        [[nodiscard]] evaluation_context& evaluate(const T& context) const
        {
            return (*func)(*this, const_cast<void*>(static_cast<const void*>(&context)));
        }

        [[nodiscard]] evaluation_context& evaluate() const
        {
            return (*func)(*this, nullptr);
        }

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

        tracked_vector<op_container_t> operations;
        stack_allocator values;
        detail::eval_func_t* func;
    };

    struct fitness_t
    {
        double raw_fitness = 0;
        double standardized_fitness = 0;
        double adjusted_fitness = 0;
        i64 hits = 0;
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

            tree_t& operator*()
            {
                return ind[pos].tree;
            }

            tree_t& operator->()
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
