/*
 *  <Short Description>
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
#include <blt/gp/generators.h>
#include <blt/gp/program.h>
#include <stack>

namespace blt::gp
{
    
    struct stack
    {
        blt::gp::operator_id id;
        blt::size_t depth;
    };
    
    static inline std::stack<stack> get_base_generator(gp_program& program)
    {
        std::stack<stack> tree_generator;
        
        auto& system = program.get_typesystem();
        // select a type which has a non-empty set of non-terminals
        type base_type;
        do
        {
            base_type = system.select_type(program.get_random());
        } while (system.get_type_non_terminals(base_type.id()).empty());
        
        tree_generator.emplace(system.select_non_terminal(program.get_random(), base_type.id()), 0);
        
        return tree_generator;
    }
    
    template<typename Func>
    tree_t create_tree(Func func, gp_program& program, blt::size_t min_depth, blt::size_t max_depth)
    {
        std::stack<stack> tree_generator = get_base_generator(program);
        tree_t tree;
        
        while (!tree_generator.empty())
        {
        
        }
        
        return tree;
    }
    
    tree_t grow_generator_t::generate(gp_program& program, blt::size_t min_depth, blt::size_t max_depth)
    {
        return create_tree([]() {
        
        }, program, min_depth, max_depth);
    }
    
    tree_t full_generator_t::generate(gp_program& program, blt::size_t min_depth, blt::size_t max_depth)
    {
        return create_tree([]() {
        
        }, program, min_depth, max_depth);
    }
}