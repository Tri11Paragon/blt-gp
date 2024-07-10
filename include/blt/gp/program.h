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

#include <blt/std/ranges.h>
#include <blt/std/hashmap.h>
#include <blt/std/types.h>
#include <blt/std/utility.h>
#include <blt/std/memory.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/typesystem.h>
#include <blt/gp/operations.h>
#include <blt/gp/transformers.h>
#include <blt/gp/selection.h>
#include <blt/gp/tree.h>
#include <blt/gp/stack.h>
#include <blt/gp/config.h>
#include <blt/gp/random.h>

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
    
    struct operator_info
    {
        // types of the arguments
        std::vector<type_id> argument_types;
        // return type of this operator
        type_id return_type;
        // number of arguments for this operator
        argc_t argc;
        // function to call this operator
        detail::callable_t function;
        // function used to transfer values between stacks
        detail::transfer_t transfer;
    };
    
    struct operator_storage
    {
        // indexed from return TYPE ID, returns index of operator
        blt::expanding_buffer<std::vector<operator_id>> terminals;
        blt::expanding_buffer<std::vector<operator_id>> non_terminals;
        blt::expanding_buffer<std::vector<std::pair<operator_id, blt::size_t>>> operators_ordered_terminals;
        // indexed from OPERATOR ID (operator number)
        blt::hashset_t<operator_id> static_types;
        std::vector<operator_info> operators;
        std::vector<detail::print_func_t> print_funcs;
        std::vector<std::optional<std::string_view>> names;
    };
    
    template<typename Context = detail::empty_t>
    class operator_builder
    {
            friend class gp_program;
            
            friend class blt::gp::detail::operator_storage_test;
        
        public:
            explicit operator_builder(type_provider& system): system(system)
            {}
            
            template<typename ArgType, typename Return, typename... Args>
            operator_builder& add_operator(const operation_t<ArgType, Return(Args...)>& op, bool is_static = false)
            {
                auto return_type_id = system.get_type<Return>().id();
                auto operator_id = blt::gp::operator_id(storage.operators.size());
                
                auto& operator_list = op.get_argc() == 0 ? storage.terminals : storage.non_terminals;
                operator_list[return_type_id].push_back(operator_id);
                
                operator_info info;
                
                if constexpr (sizeof...(Args) > 0)
                {
                    (add_non_context_argument<Args>(info.argument_types), ...);
                }
                
                info.argc.argc_context = info.argc.argc = sizeof...(Args);
                info.return_type = system.get_type<Return>().id();
                
                ((std::is_same_v<detail::remove_cv_ref<Args>, Context> ? info.argc.argc -= 1 : (blt::size_t) nullptr), ...);
                
                BLT_ASSERT(info.argc.argc_context - info.argc.argc <= 1 && "Cannot pass multiple context as arguments!");
                
                info.function = op.template make_callable<Context>();
                info.transfer = [](std::optional<std::reference_wrapper<stack_allocator>> to, stack_allocator& from) {
#if BLT_DEBUG_LEVEL >= 3
                    auto value = from.pop<Return>();
                    //BLT_TRACE_STREAM << value << "\n";
                    if (to){
                        to->get().push(value);
                    }
#else
                    if (to)
                    {
                        to->get().push(from.pop<Return>());
                    } else
                    {
                        from.pop<Return>();
                    }
#endif
                
                };
                storage.operators.push_back(info);
                storage.print_funcs.push_back([](std::ostream& out, stack_allocator& stack) {
                    out << stack.pop<Return>();
                });
                storage.names.push_back(op.get_name());
                if (is_static)
                    storage.static_types.insert(operator_id);
                return *this;
            }
            
            operator_storage&& build()
            {
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
                    std::vector<std::pair<operator_id, blt::size_t>> ordered_terminals;
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
                    
                    std::sort(ordered_terminals.begin(), ordered_terminals.end(), [](const auto& a, const auto& b) {
                        return a.second > b.second;
                    });
                    
                    auto first_size = *ordered_terminals.begin();
                    auto iter = ordered_terminals.begin();
                    while (++iter != ordered_terminals.end() && iter->second == first_size.second)
                    {}
                    
                    ordered_terminals.erase(iter, ordered_terminals.end());
                    
                    storage.operators_ordered_terminals[return_type] = ordered_terminals;
                }
                
                return std::move(storage);
            }
        
        private:
            template<typename T>
            void add_non_context_argument(decltype(operator_info::argument_types)& types)
            {
                if constexpr (!std::is_same_v<Context, detail::remove_cv_ref<T>>)
                {
                    types.push_back(system.get_type<T>().id());
                }
            }
            
            type_provider& system;
            operator_storage storage;
    };
    
    class gp_program
    {
        public:
            /**
             * Note about context size: This is required as context is passed to every operator in the GP tree, this context will be provided by your
             * call to one of the evaluator functions. This was the nicest way to provide this as C++ lacks reflection
             *
             * @param system type system to use in tree generation
             * @param engine random engine to use throughout the program. TODO replace this with something better
             * @param context_size number of arguments which are always present as "context" to the GP system / operators
             */
            explicit gp_program(type_provider& system, random_t engine):
                    system(system), engine(engine)
            {}
            
            explicit gp_program(type_provider& system, random_t engine, prog_config_t config):
                    system(system), engine(engine), config(config)
            {}
            
            void generate_population(type_id root_type)
            {
                current_pop = config.pop_initializer.get().generate(
                        {*this, root_type, config.population_size, config.initial_min_tree_size, config.initial_max_tree_size});
            }
            
            template<typename Crossover, typename Mutation, typename Reproduction, typename Creation_Func = decltype(default_next_pop_creator<Crossover, Mutation, Reproduction>)>
            void create_next_generation(Crossover&& crossover_selection, Mutation&& mutation_selection, Reproduction&& reproduction_selection,
                                        Creation_Func& func = default_next_pop_creator<Crossover, Mutation, Reproduction>)
            {
                // should already be empty
                next_pop.clear();
                crossover_selection.pre_process(*this, current_pop, current_stats);
                mutation_selection.pre_process(*this, current_pop, current_stats);
                reproduction_selection.pre_process(*this, current_pop, current_stats);
                
                func(get_selector_args(), std::forward<Crossover>(crossover_selection), std::forward<Mutation>(mutation_selection),
                     std::forward<Reproduction>(reproduction_selection));
            }
            
            /**
             * takes in a lambda for the fitness evaluation function (must return a value convertable to double)
             * The lambda must accept a tree for evaluation, container for evaluation context, and a index into that container (current tree)
             *
             * tree_t&, Container&, blt::size_t
             *
             * Container must be concurrently accessible from multiple threads using operator[]
             *
             * NOTE: 0 is considered the best, in terms of standardized and adjusted fitness
             */
            template<typename Container, typename Callable>
            void evaluate_fitness(Callable&& fitness_function, Container& result_storage)
            {
               for (const auto& ind : blt::enumerate(current_pop.get_individuals()))
                   ind.second.raw_fitness = static_cast<double>(fitness_function(ind.second.tree, result_storage, ind.first));
                double min = 0;
                for (auto& ind : current_pop.get_individuals())
                {
                    if (ind.raw_fitness < min)
                        min = ind.raw_fitness;
                }
                
                double overall_fitness = 0;
                double best_fitness = 2;
                double worst_fitness = 0;
                individual* best = nullptr;
                individual* worst = nullptr;
                
                auto diff = -min;
                for (auto& ind : current_pop.get_individuals())
                {
                    auto standardized_fitness = ind.raw_fitness + diff;
                    ind.adjusted_fitness = 1.0 / (1.0 + standardized_fitness);
                    
                    if (ind.adjusted_fitness > worst_fitness)
                    {
                        worst_fitness = ind.adjusted_fitness;
                        worst = &ind;
                    }
                    
                    if (ind.adjusted_fitness < best_fitness)
                    {
                        best_fitness = ind.adjusted_fitness;
                        best = &ind;
                    }
                    
                    overall_fitness += ind.adjusted_fitness;
                }
                
                current_stats = {overall_fitness, overall_fitness / static_cast<double>(config.population_size), best_fitness, worst_fitness, best,
                                 worst};
            }
            
            void next_generation()
            {
                current_pop = std::move(next_pop);
                current_generation++;
            }
            
            template<blt::size_t size>
            std::array<blt::size_t, size> get_best_indexes()
            {
                std::array<blt::size_t, size> arr;
                
                std::vector<std::pair<blt::size_t, double>> values;
                values.reserve(current_pop.get_individuals().size());
                
                for (const auto& ind : blt::enumerate(current_pop.get_individuals()))
                    values.emplace_back(ind.first, ind.second.adjusted_fitness);
                
                std::sort(values.begin(), values.end(), [](const auto& a, const auto& b) {
                    return a.second < b.second;
                });
                
                for (blt::size_t i = 0; i < size; i++)
                    arr[i] = values[i].first;
                
                return arr;
            }
            
            template<blt::size_t size>
            std::array<std::reference_wrapper<tree_t>, size> get_best()
            {
                return convert_array(get_best_indexes<size>(), std::make_integer_sequence<blt::size_t, size>());
            }
            
            [[nodiscard]] bool should_terminate() const
            {
                return current_generation >= config.max_generations;
            }
            
            [[nodiscard]] inline random_t& get_random()
            {
                return engine;
            }
            
            [[nodiscard]] inline type_provider& get_typesystem()
            {
                return system;
            }
            
            inline operator_id select_terminal(type_id id)
            {
                // we wanted a terminal, but could not find one, so we will select from a function that has a terminal
                if (storage.terminals[id].empty())
                    return select_non_terminal_too_deep(id);
                return storage.terminals[id][engine.get_size_t(0, storage.terminals[id].size())];
            }
            
            inline operator_id select_non_terminal(type_id id)
            {
                return storage.non_terminals[id][engine.get_size_t(0, storage.non_terminals[id].size())];
            }
            
            inline operator_id select_non_terminal_too_deep(type_id id)
            {
                return storage.operators_ordered_terminals[id][engine.get_size_t(0, storage.operators_ordered_terminals[id].size())].first;
            }
            
            inline operator_info& get_operator_info(operator_id id)
            {
                return storage.operators[id];
            }
            
            inline detail::print_func_t& get_print_func(operator_id id)
            {
                return storage.print_funcs[id];
            }
            
            inline std::optional<std::string_view> get_name(operator_id id)
            {
                return storage.names[id];
            }
            
            inline std::vector<operator_id>& get_type_terminals(type_id id)
            {
                return storage.terminals[id];
            }
            
            inline std::vector<operator_id>& get_type_non_terminals(type_id id)
            {
                return storage.non_terminals[id];
            }
            
            inline bool is_static(operator_id id)
            {
                return storage.static_types.contains(static_cast<blt::size_t>(id));
            }
            
            inline void set_operations(operator_storage&& op)
            {
                storage = std::move(op);
            }
            
            [[nodiscard]] inline auto get_current_generation() const
            {
                return current_generation;
            }
        
        private:
            type_provider& system;
            
            blt::gp::stack_allocator alloc;
            
            operator_storage storage;
            population_t current_pop;
            population_stats current_stats;
            population_t next_pop;
            blt::size_t current_generation = 0;
            
            random_t engine;
            prog_config_t config;
            
            inline selector_args get_selector_args()
            {
                return {*this, next_pop, current_pop, current_stats, config, engine};
            }
            
            template<blt::size_t size, blt::size_t... indexes>
            inline std::array<std::reference_wrapper<tree_t>, size> convert_array(std::array<blt::size_t, size>&& arr,
                                                                                  std::integer_sequence<blt::size_t, indexes...>)
            {
                return {current_pop.get_individuals()[arr[indexes]].tree...};
            }
    };
    
}

#endif //BLT_GP_PROGRAM_H
