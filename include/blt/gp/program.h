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

#ifndef BLT_GP_PROGRAM_H
#define BLT_GP_PROGRAM_H


#include <cstddef>
#include <functional>
#include <type_traits>
#include <string_view>
#include <string>
#include <utility>
#include <iostream>
#include <algorithm>
#include <memory>
#include <array>
#include <thread>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <stdexcept>

#include <blt/std/ranges.h>
#include <blt/std/hashmap.h>
#include <blt/std/types.h>
#include <blt/std/utility.h>
#include <blt/meta/meta.h>
#include <blt/std/memory.h>
#include <blt/std/thread.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/typesystem.h>
#include <blt/gp/operations.h>
#include <blt/gp/transformers.h>
#include <blt/gp/selection.h>
#include <blt/gp/tree.h>
#include <blt/gp/stack.h>
#include <blt/gp/config.h>
#include <blt/gp/random.h>
#include "blt/format/format.h"

namespace blt::gp
{
    struct argc_t
    {
        blt::u32 argc = 0;
        blt::u32 argc_context = 0;

        [[nodiscard]] bool is_terminal() const
        {
            return argc == 0;
        }
    };

    struct operator_info_t
    {
        // types of the arguments
        tracked_vector<type_id> argument_types;
        // return type of this operator
        type_id return_type;
        // number of arguments for this operator
        argc_t argc;
        // per operator function callable (slow)
        detail::operator_func_t func;
    };

    struct operator_metadata_t
    {
        blt::size_t arg_size_bytes = 0;
        blt::size_t return_size_bytes = 0;
        argc_t argc{};
    };

    struct program_operator_storage_t
    {
        // indexed from return TYPE ID, returns index of operator
        blt::expanding_buffer<tracked_vector<operator_id>> terminals;
        blt::expanding_buffer<tracked_vector<operator_id>> non_terminals;
        blt::expanding_buffer<tracked_vector<std::pair<operator_id, blt::size_t>>> operators_ordered_terminals;
        // indexed from OPERATOR ID (operator number)
        blt::hashset_t<operator_id> ephemeral_leaf_operators;
        tracked_vector<operator_info_t> operators;
        tracked_vector<operator_metadata_t> operator_metadata;
        tracked_vector<detail::print_func_t> print_funcs;
        tracked_vector<detail::destroy_func_t> destroy_funcs;
        tracked_vector<std::optional<std::string_view>> names;

        detail::eval_func_t eval_func;

        type_provider system;
    };

    template <typename Context = detail::empty_t>
    class operator_builder
    {
        friend class gp_program;

        friend class blt::gp::detail::operator_storage_test;

    public:
        explicit operator_builder() = default;

        template <typename... Operators>
        program_operator_storage_t& build(Operators&... operators)
        {
            blt::size_t largest_args = 0;
            blt::size_t largest_returns = 0;
            blt::u32 largest_argc = 0;
            operator_metadata_t meta;
            ((meta = add_operator(operators), largest_argc = std::max(meta.argc.argc, largest_argc),
                largest_args = std::max(meta.arg_size_bytes, largest_args), largest_returns = std::max(meta.return_size_bytes,
                    largest_returns)), ...);

            //                largest = largest * largest_argc;
            blt::size_t largest = largest_args * largest_argc * largest_returns * largest_argc;

            storage.eval_func = [&operators..., largest](const tree_t& tree, void* context) -> evaluation_context& {
                const auto& ops = tree.get_operations();
                const auto& vals = tree.get_values();

                static thread_local evaluation_context results{};
                results.values.reset();
                results.values.reserve(largest);

                blt::size_t total_so_far = 0;
                blt::size_t op_pos = 0;

                for (const auto& operation : blt::reverse_iterate(ops.begin(), ops.end()))
                {
                    op_pos++;
                    if (operation.is_value)
                    {
                        total_so_far += stack_allocator::aligned_size(operation.type_size);
                        results.values.copy_from(vals.from(total_so_far), stack_allocator::aligned_size(operation.type_size));
                        continue;
                    }
                    call_jmp_table(operation.id, context, results.values, results.values, operators...);
                }

                return results;
            };

            blt::hashset_t<type_id> has_terminals;

            for (const auto& v : blt::enumerate(storage.terminals))
            {
                if (!v.second.empty())
                    has_terminals.insert(v.first);
            }

            for (const auto& op_r : blt::enumerate(storage.non_terminals))
            {
                if (op_r.second.empty())
                    continue;
                auto return_type = op_r.first;
                tracked_vector<std::pair<operator_id, blt::size_t>> ordered_terminals;
                for (const auto& op : op_r.second)
                {
                    // count number of terminals
                    blt::size_t terminals = 0;
                    for (const auto& type : storage.operators[op].argument_types)
                    {
                        if (has_terminals.contains(type))
                            terminals++;
                    }
                    ordered_terminals.emplace_back(op, terminals);
                }
                bool found_terminal_inputs = false;
                bool matches_argc = false;
                for (const auto& terms : ordered_terminals)
                {
                    if (terms.second == storage.operators[terms.first].argc.argc)
                        matches_argc = true;
                    if (terms.second != 0)
                        found_terminal_inputs = true;
                    if (matches_argc && found_terminal_inputs)
                        break;
                }
                if (!found_terminal_inputs)
                    BLT_ABORT(("Failed to find function with terminal arguments for return type " + std::to_string(return_type)).c_str());
                if (!matches_argc)
                {
                    BLT_ABORT(("Failed to find a function which purely translates types "
                        "(that is all input types are terminals) for return type " + std::to_string(return_type)).c_str());
                }

                std::sort(ordered_terminals.begin(), ordered_terminals.end(), [](const auto& a, const auto& b)
                {
                    return a.second > b.second;
                });

                auto first_size = *ordered_terminals.begin();
                auto iter = ordered_terminals.begin();
                while (++iter != ordered_terminals.end() && iter->second == first_size.second)
                {
                }

                ordered_terminals.erase(iter, ordered_terminals.end());

                storage.operators_ordered_terminals[return_type] = ordered_terminals;
            }

            return storage;
        }

        program_operator_storage_t&& grab()
        {
            return std::move(storage);
        }

    private:
        template <typename RawFunction, typename Return, typename... Args>
        auto add_operator(operation_t<RawFunction, Return(Args...)>& op)
        {
            // check for types we can register
            (storage.system.register_type<Args>(), ...);
            storage.system.register_type<Return>();

            auto return_type_id = storage.system.get_type<Return>().id();
            auto operator_id = blt::gp::operator_id(storage.operators.size());
            op.id = operator_id;

            operator_info_t info;

            if constexpr (sizeof...(Args) > 0)
            {
                (add_non_context_argument<detail::remove_cv_ref<Args>>(info.argument_types), ...);
            }

            info.argc.argc_context = info.argc.argc = sizeof...(Args);
            info.return_type = return_type_id;
            info.func = op.template make_callable<Context>();

            ((std::is_same_v<detail::remove_cv_ref<Args>, Context> ? info.argc.argc -= 1 : (blt::size_t)nullptr), ...);

            auto& operator_list = info.argc.argc == 0 ? storage.terminals : storage.non_terminals;
            operator_list[return_type_id].push_back(operator_id);

            BLT_ASSERT(info.argc.argc_context - info.argc.argc <= 1 && "Cannot pass multiple context as arguments!");

            storage.operators.push_back(info);

            operator_metadata_t meta;
            if constexpr (sizeof...(Args) != 0)
            {
                meta.arg_size_bytes = (stack_allocator::aligned_size(sizeof(Args)) + ...);
            }
            meta.return_size_bytes = sizeof(Return);
            meta.argc = info.argc;

            storage.operator_metadata.push_back(meta);
            storage.print_funcs.push_back([&op](std::ostream& out, stack_allocator& stack)
            {
                if constexpr (blt::meta::is_streamable_v<Return>)
                {
                    out << stack.from<Return>(0);
                    (void)(op); // remove warning
                }
                else
                {
                    out << "[Printing Value on '" << (op.get_name() ? *op.get_name() : "") << "' Not Supported!]";
                }
            });
            storage.destroy_funcs.push_back([](detail::destroy_t type, stack_allocator& alloc)
            {
                switch (type)
                {
                case detail::destroy_t::ARGS:
                    alloc.call_destructors<Args...>();
                    break;
                case detail::destroy_t::RETURN:
                    if constexpr (detail::has_func_drop_v<remove_cvref_t<Return>>)
                    {
                        alloc.from<detail::remove_cv_ref<Return>>(0).drop();
                    }
                    break;
                }
            });
            storage.names.push_back(op.get_name());
            if (op.is_ephemeral())
                storage.ephemeral_leaf_operators.insert(operator_id);
            return meta;
        }

        template <typename T>
        void add_non_context_argument(decltype(operator_info_t::argument_types)& types)
        {
            if constexpr (!std::is_same_v<Context, detail::remove_cv_ref<T>>)
            {
                types.push_back(storage.system.get_type<T>().id());
            }
        }

        template <typename Operator>
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

        template <blt::size_t id, typename Operator>
        static bool call(blt::size_t op, void* context, stack_allocator& write_stack, stack_allocator& read_stack, Operator& operation)
        {
            if (id == op)
            {
                execute(context, write_stack, read_stack, operation);
                return false;
            }
            return true;
        }

        template <typename... Operators, size_t... operator_ids>
        static void call_jmp_table_internal(size_t op, void* context, stack_allocator& write_stack, stack_allocator& read_stack,
                                                   std::integer_sequence<size_t, operator_ids...>, Operators&... operators)
        {
            if (op >= sizeof...(operator_ids))
            {
                BLT_UNREACHABLE;
            }
            (call<operator_ids>(op, context, write_stack, read_stack, operators) && ...);
        }

        template <typename... Operators>
        static void call_jmp_table(size_t op, void* context, stack_allocator& write_stack, stack_allocator& read_stack,
                                          Operators&... operators)
        {
            call_jmp_table_internal(op, context, write_stack, read_stack, std::index_sequence_for<Operators...>(), operators...);
        }

        program_operator_storage_t storage;
    };

    class gp_program
    {
    public:
        /**
         * Note about context size: This is required as context is passed to every operator in the GP tree, this context will be provided by your
         * call to one of the evaluator functions. This was the nicest way to provide this as C++ lacks reflection
         *
         * @param seed
         */
        explicit gp_program(blt::u64 seed): seed_func([seed] { return seed; })
        {
            create_threads();
        }

        explicit gp_program(blt::u64 seed, const prog_config_t& config): seed_func([seed] { return seed; }), config(config)
        {
            create_threads();
        }

        explicit gp_program(std::function<blt::u64()> seed_func): seed_func(std::move(seed_func))
        {
            create_threads();
        }

        explicit gp_program(std::function<blt::u64()> seed_func, const prog_config_t& config): seed_func(std::move(seed_func)), config(config)
        {
            create_threads();
        }

        ~gp_program()
        {
            thread_helper.lifetime_over = true;
            thread_helper.barrier.notify_all();
            thread_helper.thread_function_condition.notify_all();
            for (auto& thread : thread_helper.threads)
            {
                if (thread->joinable())
                    thread->join();
            }
        }

        void create_next_generation()
        {
#ifdef BLT_TRACK_ALLOCATIONS
                auto gen_alloc = blt::gp::tracker.start_measurement();
#endif
            // should already be empty
            thread_helper.next_gen_left.store(config.population_size, std::memory_order_release);
            (*thread_execution_service)(0);
#ifdef BLT_TRACK_ALLOCATIONS
                blt::gp::tracker.stop_measurement(gen_alloc);
                gen_alloc.pretty_print("Generation");
#endif
        }

        void next_generation()
        {
            std::swap(current_pop, next_pop);
            ++current_generation;
        }

        void evaluate_fitness()
        {
#ifdef BLT_TRACK_ALLOCATIONS
                auto fitness_alloc = blt::gp::tracker.start_measurement();
#endif
            evaluate_fitness_internal();
#ifdef BLT_TRACK_ALLOCATIONS
                blt::gp::tracker.stop_measurement(fitness_alloc);
                fitness_alloc.pretty_print("Fitness");
                evaluation_calls.call();
                evaluation_calls.set_value(std::max(evaluation_calls.get_value(), fitness_alloc.getAllocatedByteDifference()));
                if (fitness_alloc.getAllocatedByteDifference() > 0)
                {
                    evaluation_allocations.call(fitness_alloc.getAllocatedByteDifference());
                }
#endif
        }

        void reset_program(type_id root_type, bool eval_fitness_now = true)
        {
            current_generation = 0;
            current_pop = config.pop_initializer.get().generate(
                {*this, root_type, config.population_size, config.initial_min_tree_size, config.initial_max_tree_size});
            next_pop = population_t(current_pop);
            BLT_ASSERT_MSG(current_pop.get_individuals().size() == config.population_size,
                           ("cur pop size: " + std::to_string(current_pop.get_individuals().size())).c_str());
            BLT_ASSERT_MSG(next_pop.get_individuals().size() == config.population_size,
                           ("next pop size: " + std::to_string(next_pop.get_individuals().size())).c_str());
            if (eval_fitness_now)
                evaluate_fitness_internal();
        }

        void kill()
        {
            thread_helper.lifetime_over = true;
        }

        /**
         * takes in a reference to a function for the fitness evaluation function (must return a value convertable to double)
         * The lambda must accept a tree for evaluation, and an index (current tree)
         *
         * tree_t& current_tree, blt::size_t index_of_tree
         *
         * Container must be concurrently accessible from multiple threads using operator[]
         *
         * NOTE: 0 is considered the best, in terms of standardized fitness
         */
        template <typename FitnessFunc, typename Crossover, typename Mutation, typename Reproduction, typename CreationFunc = decltype(
                      default_next_pop_creator<Crossover, Mutation, Reproduction>)>
        void generate_population(type_id root_type, FitnessFunc& fitness_function,
                                 Crossover& crossover_selection, Mutation& mutation_selection, Reproduction& reproduction_selection,
                                 CreationFunc& func = default_next_pop_creator<Crossover, Mutation, Reproduction>, bool eval_fitness_now = true)
        {
            using LambdaReturn = typename decltype(blt::meta::lambda_helper(fitness_function))::Return;
            current_pop = config.pop_initializer.get().generate(
                {*this, root_type, config.population_size, config.initial_min_tree_size, config.initial_max_tree_size});
            next_pop = population_t(current_pop);
            BLT_ASSERT_MSG(current_pop.get_individuals().size() == config.population_size,
                           ("cur pop size: " + std::to_string(current_pop.get_individuals().size())).c_str());
            BLT_ASSERT_MSG(next_pop.get_individuals().size() == config.population_size,
                           ("next pop size: " + std::to_string(next_pop.get_individuals().size())).c_str());
            if (config.threads == 1)
            {
                BLT_INFO("Starting with single thread variant!");
                thread_execution_service = std::unique_ptr<std::function<void(blt::size_t)>>(new std::function(
                    [this, &fitness_function, &crossover_selection, &mutation_selection, &reproduction_selection, &func](blt::size_t)
                    {
                        if (thread_helper.evaluation_left > 0)
                        {
                            current_stats.normalized_fitness.clear();
                            double sum_of_prob = 0;
                            for (const auto& [index, ind] : blt::enumerate(current_pop.get_individuals()))
                            {
                                if constexpr (std::is_same_v<LambdaReturn, bool> || std::is_convertible_v<LambdaReturn, bool>)
                                {
                                    auto result = fitness_function(ind.tree, ind.fitness, index);
                                    if (result)
                                        fitness_should_exit = true;
                                }
                                else
                                    fitness_function(ind.tree, ind.fitness, index);

                                if (ind.fitness.adjusted_fitness > current_stats.best_fitness)
                                    current_stats.best_fitness = ind.fitness.adjusted_fitness;

                                if (ind.fitness.adjusted_fitness < current_stats.worst_fitness)
                                    current_stats.worst_fitness = ind.fitness.adjusted_fitness;

                                current_stats.overall_fitness = current_stats.overall_fitness + ind.fitness.adjusted_fitness;
                            }
                            for (auto& ind : current_pop)
                            {
                                auto prob = (ind.fitness.adjusted_fitness / current_stats.overall_fitness);
                                current_stats.normalized_fitness.push_back(sum_of_prob + prob);
                                sum_of_prob += prob;
                            }
                            thread_helper.evaluation_left = 0;
                        }
                        if (thread_helper.next_gen_left > 0)
                        {
                            auto args = get_selector_args();

                            crossover_selection.pre_process(*this, current_pop);
                            mutation_selection.pre_process(*this, current_pop);
                            reproduction_selection.pre_process(*this, current_pop);

                            blt::size_t start = perform_elitism(args, next_pop);

                            while (start < config.population_size)
                            {
                                tree_t& c1 = next_pop.get_individuals()[start].tree;
                                tree_t* c2 = nullptr;
                                if (start + 1 < config.population_size)
                                    c2 = &next_pop.get_individuals()[start + 1].tree;
                                start += func(args, crossover_selection, mutation_selection, reproduction_selection, c1, c2, fitness_function);
                            }

                            thread_helper.next_gen_left = 0;
                        }
                    }));
            }
            else
            {
                BLT_INFO("Starting thread execution service!");
                std::scoped_lock lock(thread_helper.thread_function_control);
                thread_execution_service = std::unique_ptr<std::function<void(blt::size_t)>>(new std::function(
                    [this, &fitness_function, &crossover_selection, &mutation_selection, &reproduction_selection, &func](blt::size_t id)
                    {
                        thread_helper.barrier.wait();
                        if (thread_helper.evaluation_left > 0)
                        {
                            while (thread_helper.evaluation_left > 0)
                            {
                                blt::size_t size = 0;
                                blt::size_t begin = 0;
                                blt::size_t end = thread_helper.evaluation_left.load(std::memory_order_relaxed);
                                do
                                {
                                    size = std::min(end, config.evaluation_size);
                                    begin = end - size;
                                }
                                while (!thread_helper.evaluation_left.compare_exchange_weak(end, end - size,
                                                                                            std::memory_order::memory_order_relaxed,
                                                                                            std::memory_order::memory_order_relaxed));
                                for (blt::size_t i = begin; i < end; i++)
                                {
                                    auto& ind = current_pop.get_individuals()[i];


                                    if constexpr (std::is_same_v<LambdaReturn, bool> || std::is_convertible_v<LambdaReturn, bool>)
                                    {
                                        auto result = fitness_function(ind.tree, ind.fitness, i);
                                        if (result)
                                            fitness_should_exit = true;
                                    }
                                    else
                                    {
                                        fitness_function(ind.tree, ind.fitness, i);
                                    }

                                    auto old_best = current_stats.best_fitness.load(std::memory_order_relaxed);
                                    while (ind.fitness.adjusted_fitness > old_best &&
                                        !current_stats.best_fitness.compare_exchange_weak(old_best, ind.fitness.adjusted_fitness,
                                                                                          std::memory_order_relaxed,
                                                                                          std::memory_order_relaxed))
                                    {
                                    }

                                    auto old_worst = current_stats.worst_fitness.load(std::memory_order_relaxed);
                                    while (ind.fitness.adjusted_fitness < old_worst &&
                                        !current_stats.worst_fitness.compare_exchange_weak(old_worst, ind.fitness.adjusted_fitness,
                                                                                           std::memory_order_relaxed,
                                                                                           std::memory_order_relaxed))
                                    {
                                    }

                                    auto old_overall = current_stats.overall_fitness.load(std::memory_order_relaxed);
                                    while (!current_stats.overall_fitness.compare_exchange_weak(old_overall,
                                                                                                ind.fitness.adjusted_fitness + old_overall,
                                                                                                std::memory_order_relaxed,
                                                                                                std::memory_order_relaxed))
                                    {
                                    }
                                }
                            }
                        }
                        if (thread_helper.next_gen_left > 0)
                        {
                            auto args = get_selector_args();
                            if (id == 0)
                            {
                                current_stats.normalized_fitness.clear();
                                double sum_of_prob = 0;
                                for (auto& ind : current_pop)
                                {
                                    auto prob = (ind.fitness.adjusted_fitness / current_stats.overall_fitness);
                                    current_stats.normalized_fitness.push_back(sum_of_prob + prob);
                                    sum_of_prob += prob;
                                }

                                crossover_selection.pre_process(*this, current_pop);
                                if (&crossover_selection != &mutation_selection)
                                    mutation_selection.pre_process(*this, current_pop);
                                if (&crossover_selection != &reproduction_selection)
                                    reproduction_selection.pre_process(*this, current_pop);
                                auto elite_amount = perform_elitism(args, next_pop);
                                thread_helper.next_gen_left -= elite_amount;
                            }
                            thread_helper.barrier.wait();

                            while (thread_helper.next_gen_left > 0)
                            {
                                blt::size_t size = 0;
                                blt::size_t begin = 0;
                                blt::size_t end = thread_helper.next_gen_left.load(std::memory_order_relaxed);
                                do
                                {
                                    size = std::min(end, config.evaluation_size);
                                    begin = end - size;
                                }
                                while (!thread_helper.next_gen_left.compare_exchange_weak(end, end - size,
                                                                                          std::memory_order::memory_order_relaxed,
                                                                                          std::memory_order::memory_order_relaxed));

                                while (begin != end)
                                {
                                    auto index = config.elites + begin;
                                    tree_t& c1 = next_pop.get_individuals()[index].tree;
                                    tree_t* c2 = nullptr;
                                    if (begin + 1 < end)
                                        c2 = &next_pop.get_individuals()[index + 1].tree;
                                    begin += func(args, crossover_selection, mutation_selection, reproduction_selection, c1, c2, fitness_function);
                                }
                            }
                        }
                        thread_helper.barrier.wait();
                    }));
                thread_helper.thread_function_condition.notify_all();
            }
            if (eval_fitness_now)
                evaluate_fitness_internal();
        }

        [[nodiscard]] bool should_terminate() const
        {
            return current_generation >= config.max_generations || fitness_should_exit;
        }

        [[nodiscard]] bool should_thread_terminate() const
        {
            return thread_helper.lifetime_over;
        }

        operator_id select_terminal(type_id id)
        {
            // we wanted a terminal, but could not find one, so we will select from a function that has a terminal
            if (storage.terminals[id].empty())
                return select_non_terminal_too_deep(id);
            return get_random().select(storage.terminals[id]);
        }

        operator_id select_non_terminal(type_id id)
        {
            // non-terminal doesn't exist, return a terminal. This is useful for types that are defined only to have a random value, nothing more.
            // was considering an std::optional<> but that would complicate the generator code considerably. I'll mark this as a TODO for v2
            if (storage.non_terminals[id].empty())
                return select_terminal(id);
            return get_random().select(storage.non_terminals[id]);
        }

        operator_id select_non_terminal_too_deep(type_id id)
        {
            // this should probably be an error.
            if (storage.operators_ordered_terminals[id].empty())
                BLT_ABORT("An impossible state has been reached. Please consult the manual. Error 43");
            return get_random().select(storage.operators_ordered_terminals[id]).first;
        }

        auto& get_current_pop()
        {
            return current_pop;
        }

        [[nodiscard]] random_t& get_random() const;

        [[nodiscard]] type_provider& get_typesystem()
        {
            return storage.system;
        }

        [[nodiscard]] operator_info_t& get_operator_info(operator_id id)
        {
            return storage.operators[id];
        }

        [[nodiscard]] detail::print_func_t& get_print_func(operator_id id)
        {
            return storage.print_funcs[id];
        }

        [[nodiscard]] detail::destroy_func_t& get_destroy_func(operator_id id)
        {
            return storage.destroy_funcs[id];
        }

        [[nodiscard]] std::optional<std::string_view> get_name(operator_id id)
        {
            return storage.names[id];
        }

        [[nodiscard]] tracked_vector<operator_id>& get_type_terminals(type_id id)
        {
            return storage.terminals[id];
        }

        [[nodiscard]] tracked_vector<operator_id>& get_type_non_terminals(type_id id)
        {
            return storage.non_terminals[id];
        }

        [[nodiscard]] detail::eval_func_t& get_eval_func()
        {
            return storage.eval_func;
        }

        [[nodiscard]] auto get_current_generation() const
        {
            return current_generation.load();
        }

        [[nodiscard]] const auto& get_population_stats() const
        {
            return current_stats;
        }

        [[nodiscard]] bool is_operator_ephemeral(operator_id id)
        {
            return storage.ephemeral_leaf_operators.contains(static_cast<blt::size_t>(id));
        }

        void set_operations(program_operator_storage_t op)
        {
            storage = std::move(op);
        }

        template <blt::size_t size>
        std::array<blt::size_t, size> get_best_indexes()
        {
            std::array<blt::size_t, size> arr;

            tracked_vector<std::pair<blt::size_t, double>> values;
            values.reserve(current_pop.get_individuals().size());

            for (const auto& ind : blt::enumerate(current_pop.get_individuals()))
                values.emplace_back(ind.first, ind.second.fitness.adjusted_fitness);

            std::sort(values.begin(), values.end(), [](const auto& a, const auto& b)
            {
                return a.second > b.second;
            });

            for (blt::size_t i = 0; i < std::min(size, config.population_size); i++)
                arr[i] = values[i].first;
            for (blt::size_t i = std::min(size, config.population_size); i < size; i++)
                arr[i] = 0;

            return arr;
        }

        template <blt::size_t size>
        auto get_best_trees()
        {
            return convert_array<std::array<std::reference_wrapper<individual_t>, size>>(get_best_indexes<size>(),
                                                                                         [this](auto&& arr, blt::size_t index) -> tree_t& {
                                                                                             return current_pop.get_individuals()[arr[index]].tree;
                                                                                         },
                                                                                         std::make_integer_sequence<blt::size_t, size>());
        }

        template <blt::size_t size>
        auto get_best_individuals()
        {
            return convert_array<std::array<std::reference_wrapper<individual_t>, size>>(get_best_indexes<size>(),
                                                                                         [this](auto&& arr, blt::size_t index) -> individual_t& {
                                                                                             return current_pop.get_individuals()[arr[index]];
                                                                                         },
                                                                                         std::make_integer_sequence<blt::size_t, size>());
        }

    private:
        selector_args get_selector_args()
        {
            return {*this, current_pop, current_stats, config, get_random()};
        }

        template <typename Return, blt::size_t size, typename Accessor, blt::size_t... indexes>
        Return convert_array(std::array<blt::size_t, size>&& arr, Accessor&& accessor,
                                    std::integer_sequence<blt::size_t, indexes...>)
        {
            return Return{accessor(arr, indexes)...};
        }

        void create_threads();

        void evaluate_fitness_internal()
        {
            statistic_history.push_back(current_stats);
            current_stats.clear();
            thread_helper.evaluation_left.store(config.population_size, std::memory_order_release);
            (*thread_execution_service)(0);

            current_stats.average_fitness = current_stats.overall_fitness / static_cast<double>(config.population_size);
        }

    private:
        program_operator_storage_t storage;
        std::function<u64()> seed_func;
        prog_config_t config{};

        population_t current_pop;
        population_t next_pop;

        std::atomic_uint64_t current_generation = 0;

        std::atomic_bool fitness_should_exit = false;

        population_stats current_stats{};
        tracked_vector<population_stats> statistic_history;

        struct concurrency_storage
        {
            std::vector<std::unique_ptr<std::thread>> threads;

            std::mutex thread_function_control{};
            std::condition_variable thread_function_condition{};

            std::atomic_uint64_t evaluation_left = 0;
            std::atomic_uint64_t next_gen_left = 0;

            std::atomic_bool lifetime_over = false;
            blt::barrier barrier;

            explicit concurrency_storage(blt::size_t threads): barrier(threads, lifetime_over)
            {
            }
        } thread_helper{config.threads == 0 ? std::thread::hardware_concurrency() : config.threads};

        std::unique_ptr<std::function<void(blt::size_t)>> thread_execution_service = nullptr;
    };
}

#endif //BLT_GP_PROGRAM_H
