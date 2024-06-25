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
    
    inline std::stack<stack> get_initial_stack(gp_program& program)
    {
        std::stack<stack> tree_generator;
        
        auto& system = program.get_typesystem();
        // select a type which has a non-empty set of non-terminals
        type base_type;
        do
        {
            base_type = system.select_type(program.get_random());
        } while (program.get_type_non_terminals(base_type.id()).empty());
        
        tree_generator.push(stack{program.select_non_terminal(base_type.id()), 1});
        
        return tree_generator;
    }
    
    template<typename Func>
    inline tree_t create_tree(Func&& perChild, gp_program& program)
    {
        std::stack<stack> tree_generator = get_initial_stack(program);
        blt::size_t max_depth = 0;
        tree_t tree;
        
        while (!tree_generator.empty())
        {
            auto top = tree_generator.top();
            tree_generator.pop();
            
            tree.get_operations().push_back({top.id, static_cast<blt::u16>(top.depth)});
            max_depth = std::max(max_depth, top.depth);
            
            if (program.is_static(top.id))
            {
                program.get_operation(top.id)(nullptr, tree.get_values());
                continue;
            }
            
            for (const auto& child : program.get_argument_types(top.id))
            {
                std::forward<Func>(perChild)(program, tree_generator, child, top.depth + 1);
            }
        }
        
        tree.setDepth(max_depth);
        
        return tree;
    }
    
    tree_t grow_generator_t::generate(gp_program& program, blt::size_t min_depth, blt::size_t max_depth)
    {
        return create_tree([min_depth, max_depth](gp_program& program, std::stack<stack>& tree_generator, const type& type, blt::size_t new_depth) {
            if (new_depth >= max_depth)
            {
                tree_generator.push({program.select_terminal(type.id()), new_depth});
                return;
            }
            if (program.choice() || new_depth < min_depth)
                tree_generator.push({program.select_non_terminal(type.id()), new_depth});
            else
                tree_generator.push({program.select_terminal(type.id()), new_depth});
        }, program);
    }
    
    tree_t full_generator_t::generate(gp_program& program, blt::size_t, blt::size_t max_depth)
    {
        return create_tree([max_depth](gp_program& program, std::stack<stack>& tree_generator, const type& type, blt::size_t new_depth) {
            if (new_depth >= max_depth)
            {
                tree_generator.push({program.select_terminal(type.id()), new_depth});
                return;
            }
            tree_generator.push({program.select_non_terminal(type.id()), new_depth});
        }, program);
    }
    
    population_t grow_initializer_t::generate(gp_program& program, blt::size_t size, blt::size_t min_depth, blt::size_t max_depth)
    {
        return population_t();
    }
    
    population_t full_initializer_t::generate(gp_program& program, blt::size_t size, blt::size_t min_depth, blt::size_t max_depth)
    {
        return population_t();
    }
    
    population_t half_half_initializer_t::generate(gp_program& program, blt::size_t size, blt::size_t min_depth, blt::size_t max_depth)
    {
        return population_t();
    }
    
    population_t ramped_half_initializer_t::generate(gp_program& program, blt::size_t size, blt::size_t min_depth, blt::size_t max_depth)
    {
        return population_t();
    }
}