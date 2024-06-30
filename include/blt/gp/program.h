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
#include <random>
#include <algorithm>

#include <blt/std/ranges.h>
#include <blt/std/hashmap.h>
#include <blt/std/types.h>
#include <blt/std/utility.h>
#include <blt/std/memory.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/typesystem.h>
#include <blt/gp/operations.h>
#include <blt/gp/tree.h>
#include <blt/gp/stack.h>

namespace blt::gp
{
    static constexpr blt::size_t NONE_T = 0x0;
    static constexpr blt::size_t STATIC_T = 0x1;
    static constexpr blt::size_t TERMINAL_T = 0x2;
    
    struct argc_t
    {
        blt::u32 argc = 0;
        blt::u32 argc_context = 0;
    };
    
    struct operator_info
    {
        std::vector<type_id> argument_types;
        type_id return_type;
        argc_t argc;
        detail::callable_t function;
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
        blt::expanding_buffer<std::vector<type>> argument_types;
        blt::expanding_buffer<argc_t> operator_argc;
        std::vector<detail::callable_t> operators;
        std::vector<detail::transfer_t> transfer_funcs;
    };
    
    template<typename Context = detail::empty_t>
    class operator_builder
    {
            friend class gp_program;
            
            friend class blt::gp::detail::operator_storage_test;
        
        public:
            explicit operator_builder(type_provider& system): system(system)
            {}
            
            template<typename Return, typename... Args>
            operator_builder& add_operator(const operation_t<Return(Args...)>& op, bool is_static = false)
            {
                auto return_type_id = system.get_type<Return>().id();
                auto operator_id = blt::gp::operator_id(storage.operators.size());
                
                auto& operator_list = op.get_argc() == 0 ? storage.terminals : storage.non_terminals;
                operator_list[return_type_id].push_back(operator_id);
                
                if constexpr (sizeof...(Args) > 0)
                {
                    (add_non_context_argument<Args>(operator_id), ...);
                }
                
                argc_t argc;
                argc.argc_context = argc.argc = sizeof...(Args);
                
                ((std::is_same_v<detail::remove_cv_ref<Args>, Context> ? argc.argc -= 1 : (blt::size_t) nullptr), ...);
                
                BLT_ASSERT(argc.argc_context - argc.argc <= 1 && "Cannot pass multiple context as arguments!");
                
                storage.operator_argc[operator_id] = argc;
                
                storage.operators.push_back(op.template make_callable<Context>());
                storage.transfer_funcs.push_back([](stack_allocator& to, stack_allocator& from, blt::ptrdiff_t offset) {
                    if (offset < 0)
                        to.push(from.pop<Return>());
                    else
                        to.push(from.from<Return>(offset));
                });
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
                        for (const auto& type : storage.argument_types[op])
                        {
                            if (has_terminals.contains(type.id()))
                                terminals++;
                        }
                        ordered_terminals.emplace_back(op, terminals);
                    }
                    bool found_terminal_inputs = false;
                    bool matches_argc = false;
                    for (const auto& terms : ordered_terminals)
                    {
                        if (terms.second == storage.operator_argc[terms.first].argc)
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
            void add_non_context_argument(blt::gp::operator_id operator_id)
            {
                if constexpr (!std::is_same_v<Context, detail::remove_cv_ref<T>>)
                {
                    storage.argument_types[operator_id].push_back(system.get_type<T>());
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
            explicit gp_program(type_provider& system, std::mt19937_64 engine):
                    system(system), engine(engine)
            {}
            
            void generate_tree();
            
            [[nodiscard]] inline std::mt19937_64& get_random()
            {
                return engine;
            }
            
            [[nodiscard]] inline bool choice()
            {
                static std::uniform_int_distribution dist(0, 1);
                return dist(engine);
            }
            
            /**
             * @param cutoff precent in floating point form chance of the event happening.
             * @return
             */
            [[nodiscard]] inline bool choice(double cutoff)
            {
                static std::uniform_real_distribution dist(0.0, 1.0);
                return dist(engine) < cutoff;
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
                std::uniform_int_distribution<blt::size_t> dist(0, storage.terminals[id].size() - 1);
                return storage.terminals[id][dist(engine)];
            }
            
            inline operator_id select_non_terminal(type_id id)
            {
                std::uniform_int_distribution<blt::size_t> dist(0, storage.non_terminals[id].size() - 1);
                return storage.non_terminals[id][dist(engine)];
            }
            
            inline operator_id select_non_terminal_too_deep(type_id id)
            {
                std::uniform_int_distribution<blt::size_t> dist(0, storage.operators_ordered_terminals[id].size() - 1);
                return storage.operators_ordered_terminals[id][dist(engine)].first;
            }
            
            inline std::vector<type>& get_argument_types(operator_id id)
            {
                return storage.argument_types[id];
            }
            
            inline std::vector<operator_id>& get_type_terminals(type_id id)
            {
                return storage.terminals[id];
            }
            
            inline std::vector<operator_id>& get_type_non_terminals(type_id id)
            {
                return storage.non_terminals[id];
            }
            
            inline argc_t get_argc(operator_id id)
            {
                return storage.operator_argc[id];
            }
            
            inline detail::callable_t& get_operation(operator_id id)
            {
                return storage.operators[id];
            }
            
            inline detail::transfer_t& get_transfer_func(operator_id id)
            {
                return storage.transfer_funcs[id];
            }
            
            inline bool is_static(operator_id id)
            {
                return storage.static_types.contains(static_cast<blt::size_t>(id));
            }
            
            inline void set_operations(operator_storage&& op)
            {
                storage = std::move(op);
            }
        
        private:
            type_provider& system;
            blt::gp::stack_allocator alloc;
            
            operator_storage storage;
            
            std::mt19937_64 engine;
    };
    
}

#endif //BLT_GP_PROGRAM_H
