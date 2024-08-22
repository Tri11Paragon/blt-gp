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
#include <blt/std/logging.h>
#include <stack>

namespace blt::gp
{
    // TODO: change how the generators work, but keep how nice everything is within the C++ file. less headers!
    // maybe have tree call the generate functions with out variables as the members of tree_t
    
    struct stack
    {
        blt::gp::operator_id id;
        blt::size_t depth;
    };
    
    inline std::stack<stack> get_initial_stack(gp_program& program, type_id root_type)
    {
        std::stack<stack> tree_generator;
//
//        auto& system = program.get_typesystem();
//        // select a type which has a non-empty set of non-terminals
//        type base_type;
//        do
//        {
//            base_type = system.select_type(program.get_random());
//        } while (program.get_type_non_terminals(base_type.id()).empty());
//
        tree_generator.push(stack{program.select_non_terminal(root_type), 1});
        
        return tree_generator;
    }
    
    template<typename Func>
    inline tree_t create_tree(Func&& perChild, const generator_arguments& args)
    {
        std::stack<stack> tree_generator = get_initial_stack(args.program, args.root_type);
        blt::size_t max_depth = 0;
        tree_t tree{args.program};
        
        while (!tree_generator.empty())
        {
            auto top = tree_generator.top();
            tree_generator.pop();
            
            auto& info = args.program.get_operator_info(top.id);
            
            tree.get_operations().emplace_back(
                    args.program.get_typesystem().get_type(info.return_type).size(),
                    top.id,
                    args.program.is_static(top.id));
            max_depth = std::max(max_depth, top.depth);
            
            if (args.program.is_static(top.id))
            {
                info.func(nullptr, tree.get_values(), tree.get_values());
                continue;
            }
            
            for (const auto& child : info.argument_types)
                std::forward<Func>(perChild)(args.program, tree_generator, child, top.depth + 1);
        }
        
        return tree;
    }
    
    tree_t grow_generator_t::generate(const generator_arguments& args)
    {
        return create_tree([args](gp_program& program, std::stack<stack>& tree_generator, type_id type, blt::size_t new_depth) {
            if (new_depth >= args.max_depth)
            {
                if (program.get_type_terminals(type).empty())
                    tree_generator.push({program.select_non_terminal_too_deep(type), new_depth});
                else
                    tree_generator.push({program.select_terminal(type), new_depth});
                return;
            }
            if (program.get_random().choice() || new_depth < args.min_depth)
                tree_generator.push({program.select_non_terminal(type), new_depth});
            else
                tree_generator.push({program.select_terminal(type), new_depth});
        }, args);
    }
    
    tree_t full_generator_t::generate(const generator_arguments& args)
    {
        return create_tree([args](gp_program& program, std::stack<stack>& tree_generator, type_id type, blt::size_t new_depth) {
            if (new_depth >= args.max_depth)
            {
                if (program.get_type_terminals(type).empty())
                    tree_generator.push({program.select_non_terminal_too_deep(type), new_depth});
                else
                    tree_generator.push({program.select_terminal(type), new_depth});
                return;
            }
            tree_generator.push({program.select_non_terminal(type), new_depth});
        }, args);
    }
    
    population_t grow_initializer_t::generate(const initializer_arguments& args)
    {
        population_t pop;
        
        for (auto i = 0ul; i < args.size; i++)
            pop.get_individuals().emplace_back(grow.generate(args.to_gen_args()));
        
        return pop;
    }
    
    population_t full_initializer_t::generate(const initializer_arguments& args)
    {
        population_t pop;
        
        for (auto i = 0ul; i < args.size; i++)
            pop.get_individuals().emplace_back(full.generate(args.to_gen_args()));
        
        return pop;
    }
    
    population_t half_half_initializer_t::generate(const initializer_arguments& args)
    {
        population_t pop;
        
        for (auto i = 0ul; i < args.size; i++)
        {
            if (args.program.get_random().choice())
                pop.get_individuals().emplace_back(full.generate(args.to_gen_args()));
            else
                pop.get_individuals().emplace_back(grow.generate(args.to_gen_args()));
        }
        
        return pop;
    }
    
    population_t ramped_half_initializer_t::generate(const initializer_arguments& args)
    {
        auto steps = args.max_depth - args.min_depth;
        auto per_step = args.size / steps;
        auto remainder = args.size % steps;
        population_t pop;
        
        for (auto depth : blt::range(args.min_depth, args.max_depth))
        {
            for (auto i = 0ul; i < per_step; i++)
            {
                if (args.program.get_random().choice())
                    pop.get_individuals().emplace_back(full.generate({args.program, args.root_type, args.min_depth, depth}));
                else
                    pop.get_individuals().emplace_back(grow.generate({args.program, args.root_type, args.min_depth, depth}));
            }
        }
        
        for (auto i = 0ul; i < remainder; i++)
        {
            if (args.program.get_random().choice())
                pop.get_individuals().emplace_back(full.generate(args.to_gen_args()));
            else
                pop.get_individuals().emplace_back(grow.generate(args.to_gen_args()));
        }
        
        blt_assert(pop.get_individuals().size() == args.size);
        
        return pop;
    }
}