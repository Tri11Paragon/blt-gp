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
    class gp_program
    {
        public:
            explicit gp_program(type_system system): system(std::move(system))
            {}
            
            template<typename Return, typename... Args>
            void add_operator(const operation_t<Return(Args...)>& op)
            {
                auto return_type_id = system.get_type<Return>().id();
                auto& operator_list = op.get_argc() == 0 ? terminals : non_terminals;
                operator_list[return_type_id].push_back(operators.size());
                
                auto operator_index = operators.size();
                (argument_types[operator_index].push_back(system.get_type<Args>()), ...);
                operators.push_back(op.make_callable());
            }
            
            [[nodiscard]] inline type_system& get_typesystem()
            {
                return system;
            }
            
            void generate_tree();
            
            inline operator_id select_terminal(type_id id, std::mt19937_64& engine)
            {
                std::uniform_int_distribution<blt::size_t> dist(0, terminals[id].size() - 1);
                return terminals[id][dist(engine)];
            }
            
            inline operator_id select_non_terminal(type_id id, std::mt19937_64& engine)
            {
                std::uniform_int_distribution<blt::size_t> dist(0, non_terminals[id].size() - 1);
                return non_terminals[id][dist(engine)];
            }
            
            inline std::vector<type>& get_argument_types(operator_id id)
            {
                return argument_types[id];
            }
        
        private:
            type_system system;
            blt::gp::stack_allocator alloc;
            // indexed from return TYPE ID, returns index of operator
            blt::expanding_buffer<std::vector<operator_id>> terminals;
            blt::expanding_buffer<std::vector<operator_id>> non_terminals;
            // indexed from OPERATOR ID (operator number)
            blt::expanding_buffer<std::vector<type>> argument_types;
            std::vector<std::function<void(stack_allocator&)>> operators;
    };
    
}

#endif //BLT_GP_PROGRAM_H
