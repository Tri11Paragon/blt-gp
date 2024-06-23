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

#ifndef BLT_GP_TYPESYSTEM_H
#define BLT_GP_TYPESYSTEM_H

#include <blt/std/hashmap.h>
#include <blt/std/types.h>
#include <blt/std/ranges.h>
#include <blt/std/utility.h>
#include <blt/std/memory.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/stack.h>
#include <blt/gp/operations.h>
#include <random>

namespace blt::gp
{
    struct operator_id : integer_type<blt::size_t>
    {
        using integer_type<blt::size_t>::integer_type;
    };
    
    struct type_id : integer_type<blt::size_t>
    {
        using integer_type<blt::size_t>::integer_type;
    };
    
    class type
    {
        public:
            type() = default;
            
            template<typename T>
            static type make_type(type_id id)
            {
                return type(sizeof(T), id, blt::type_string<T>());
            }
            
            [[nodiscard]] inline blt::size_t size() const
            {
                return size_;
            }
            
            [[nodiscard]] inline type_id id() const
            {
                return id_;
            }
            
            [[nodiscard]] inline std::string_view name() const
            {
                return name_;
            }
        
        private:
            type(size_t size, type_id id, std::string_view name): size_(size), id_(id), name_(name)
            {}
            
            blt::size_t size_{};
            type_id id_{};
            std::string name_{};
    };
    
    /**
     * Is a provider for the set of types possible in a GP program
     * also provides a set of functions for converting between C++ types and BLT GP types
     */
    class type_system
    {
        public:
            type_system() = default;
            
            template<typename T>
            inline type register_type()
            {
                types.insert({blt::type_string_raw<T>(), type::make_type<T>(types.size())});
                return types[blt::type_string_raw<T>()];
            }
            
            template<typename T>
            inline type get_type()
            {
                return types[blt::type_string_raw<T>()];
            }
            
            /**
             * This function is slow btw
             * @param engine
             * @return
             */
            inline type select_type(std::mt19937_64& engine)
            {
                std::uniform_int_distribution dist(0ul, types.size() - 1);
                auto offset = dist(engine);
                auto itr = types.begin();
                for ([[maybe_unused]] auto _ : blt::range(0ul, offset))
                    itr = itr++;
                return itr->second;
            }
            
            inline operator_id select_terminal(std::mt19937_64& engine, type_id id)
            {
                std::uniform_int_distribution<blt::size_t> dist(0, terminals[id].size() - 1);
                return terminals[id][dist(engine)];
            }
            
            inline operator_id select_non_terminal(std::mt19937_64& engine, type_id id)
            {
                std::uniform_int_distribution<blt::size_t> dist(0, non_terminals[id].size() - 1);
                return non_terminals[id][dist(engine)];
            }
            
            inline std::vector<type>& get_argument_types(operator_id id)
            {
                return argument_types[id];
            }
            
            inline std::vector<operator_id>& get_type_terminals(type_id id)
            {
                return terminals[id];
            }
            
            inline std::vector<operator_id>& get_type_non_terminals(type_id id)
            {
                return non_terminals[id];
            }
            
            template<typename Return, typename... Args>
            void add_operator(const operation_t<Return(Args...)>& op)
            {
                auto return_type_id = get_type<Return>().id();
                auto& operator_list = op.get_argc() == 0 ? terminals : non_terminals;
                operator_list[return_type_id].push_back(operators.size());
                
                auto operator_index = operators.size();
                (argument_types[operator_index].push_back(get_type<Args>()), ...);
                operators.push_back(op.make_callable());
            }
        
        private:
            blt::hashmap_t<std::string, type> types;
            // indexed from return TYPE ID, returns index of operator
            blt::expanding_buffer<std::vector<operator_id>> terminals;
            blt::expanding_buffer<std::vector<operator_id>> non_terminals;
            // indexed from OPERATOR ID (operator number)
            blt::expanding_buffer<std::vector<type>> argument_types;
            std::vector<std::function<void(stack_allocator&)>> operators;
    };
}

#endif //BLT_GP_TYPESYSTEM_H
