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
#include <blt/gp/transformers.h>
#include <blt/gp/program.h>
#include <blt/std/ranges.h>
#include <random>

namespace blt::gp
{
    grow_generator_t grow_generator;
    
    blt::expected<crossover_t::result_t, crossover_t::error_t> crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2) // NOLINT
    {
        result_t result{p1, p2};
        
        auto& c1 = result.child1;
        auto& c2 = result.child2;
        
        auto& c1_ops = c1.get_operations();
        auto& c2_ops = c2.get_operations();
        
        if (c1_ops.size() < 5 || c2_ops.size() < 5)
            return blt::unexpected(error_t::TREE_TOO_SMALL);
        
        auto point = get_crossover_point(program, c1, c2);
        
        if (!point)
            return blt::unexpected(point.error());
        
        auto crossover_point_begin_itr = c1_ops.begin() + point->p1_crossover_point;
        auto crossover_point_end_itr = c1_ops.begin() + find_endpoint(program, c1_ops, point->p1_crossover_point);

        auto found_point_begin_itr = c2_ops.begin() + point->p2_crossover_point;
        auto found_point_end_itr = c2_ops.begin() + find_endpoint(program, c2_ops, point->p2_crossover_point);
        
        stack_allocator& c1_stack_init = c1.get_values();
        stack_allocator& c2_stack_init = c2.get_values();
        
        // we have to make a copy because we will modify the underlying storage.
        std::vector<op_container_t> c1_operators;
        std::vector<op_container_t> c2_operators;
        
        for (const auto& op : blt::iterate(crossover_point_begin_itr, crossover_point_end_itr))
            c1_operators.push_back(op);
        for (const auto& op : blt::iterate(found_point_begin_itr, found_point_end_itr))
            c2_operators.push_back(op);
        
        stack_allocator c1_stack_after_copy;
        stack_allocator c1_stack_for_copy;
        stack_allocator c2_stack_after_copy;
        stack_allocator c2_stack_for_copy;

        // transfer all values after the crossover point. these will need to be transferred back to child2
        transfer_backward(c1_stack_init, c1_stack_after_copy, c1_ops.end()-1, crossover_point_end_itr - 1);
        // transfer all values for the crossover point.
        transfer_backward(c1_stack_init, c1_stack_for_copy, crossover_point_end_itr - 1, crossover_point_begin_itr - 1);
        // transfer child2 values for copying back into c1
        transfer_backward(c2_stack_init, c2_stack_after_copy, c2_ops.end() - 1, found_point_end_itr - 1);
        transfer_backward(c2_stack_init, c2_stack_for_copy, found_point_end_itr - 1, found_point_begin_itr - 1);
        // now copy back into the respective children
        transfer_forward(c2_stack_for_copy, c1.get_values(), found_point_begin_itr, found_point_end_itr);
        transfer_forward(c1_stack_for_copy, c2.get_values(), crossover_point_begin_itr, crossover_point_end_itr);
        // now copy after the crossover point back to the correct children
        transfer_forward(c1_stack_after_copy, c1.get_values(), crossover_point_end_itr, c1_ops.end());
        transfer_forward(c2_stack_after_copy, c2.get_values(), found_point_end_itr, c2_ops.end());
        
        // now swap the operators
        auto insert_point_c1 = crossover_point_begin_itr - 1;
        auto insert_point_c2 = found_point_begin_itr - 1;
        
        // invalidates [begin, end()) so the insert points should be fine
        c1_ops.erase(crossover_point_begin_itr, crossover_point_end_itr);
        c2_ops.erase(found_point_begin_itr, found_point_end_itr);
        
        c1_ops.insert(++insert_point_c1, c2_operators.begin(), c2_operators.end());
        c2_ops.insert(++insert_point_c2, c1_operators.begin(), c1_operators.end());
        
        return result;
    }
    
    blt::expected<crossover_t::crossover_point_t, crossover_t::error_t> crossover_t::get_crossover_point(gp_program& program, const tree_t& c1,
                                                                                                         const tree_t& c2) const
    {
        auto& c1_ops = c1.get_operations();
        auto& c2_ops = c2.get_operations();
        
        blt::size_t crossover_point = program.get_random().get_size_t(1ul, c1_ops.size());
        
        while (config.avoid_terminals && program.get_operator_info(c1_ops[crossover_point].id).argc.is_terminal())
            crossover_point = program.get_random().get_size_t(1ul, c1_ops.size());
        
        blt::size_t attempted_point = 0;
        
        const auto& crossover_point_type = program.get_operator_info(c1_ops[crossover_point].id);
        operator_info* attempted_point_type = nullptr;
        
        blt::size_t counter = 0;
        do
        {
            if (counter >= config.max_crossover_tries)
            {
                if (config.should_crossover_try_forward)
                {
                    bool found = false;
                    for (auto i = attempted_point + 1; i < c2_ops.size(); i++)
                    {
                        auto* info = &program.get_operator_info(c2_ops[i].id);
                        if (info->return_type == crossover_point_type.return_type)
                        {
                            if (config.avoid_terminals && info->argc.is_terminal())
                                continue;
                            attempted_point = i;
                            attempted_point_type = info;
                            found = true;
                            break;
                        }
                    }
                    if (!found)
                        return blt::unexpected(error_t::NO_VALID_TYPE);
                }
                // should we try again over the whole tree? probably not.
                return blt::unexpected(error_t::NO_VALID_TYPE);
            } else
            {
                attempted_point = program.get_random().get_size_t(1ul, c2_ops.size());
                attempted_point_type = &program.get_operator_info(c2_ops[attempted_point].id);
                if (config.avoid_terminals && attempted_point_type->argc.is_terminal())
                    continue;
                if (crossover_point_type.return_type == attempted_point_type->return_type)
                    break;
                counter++;
            }
        } while (true);
        
        return crossover_point_t{static_cast<blt::ptrdiff_t>(crossover_point), static_cast<blt::ptrdiff_t>(attempted_point)};
    }
    
    tree_t mutation_t::apply(gp_program& program, const tree_t& p)
    {
        auto c = p;
        
        auto& ops = c.get_operations();
        auto& vals = c.get_values();
        
        auto point = static_cast<blt::ptrdiff_t>(program.get_random().get_size_t(0ul, ops.size()));
        const auto& type_info = program.get_operator_info(ops[point].id);
        
        auto begin_p = ops.begin() + point;
        auto end_p = ops.begin() + find_endpoint(program, ops, point);
        
        stack_allocator after_stack;

        transfer_backward(vals, after_stack, ops.end() - 1, end_p - 1);
        
        for (auto it = end_p - 1; it != begin_p - 1; it--)
        {
            if (it->is_value)
                vals.pop_bytes(static_cast<blt::ptrdiff_t>(it->type_size));
        }
        
        auto before = begin_p - 1;
        
        ops.erase(begin_p, end_p);
        
        auto new_tree = config.generator.get().generate({program, type_info.return_type, config.replacement_min_depth, config.replacement_max_depth});
        
        auto& new_ops = new_tree.get_operations();
        auto& new_vals = new_tree.get_values();
        
        ops.insert(++before, new_ops.begin(), new_ops.end());
        
        transfer_backward(new_vals, vals, new_ops.end() - 1, new_ops.begin() - 1);
        
        auto new_end_point = point + new_ops.size();
        auto new_end_p = ops.begin() + static_cast<blt::ptrdiff_t>(new_end_point);
        
        transfer_forward(after_stack, vals, new_end_p, ops.end());
        
        return c;
    }
    
    mutation_t::config_t::config_t(): generator(grow_generator)
    {}
    
    blt::ptrdiff_t find_endpoint(blt::gp::gp_program& program, const std::vector<blt::gp::op_container_t>& container, blt::ptrdiff_t index)
    {
        blt::i64 children_left = 0;
        
        do
        {
            const auto& type = program.get_operator_info(container[index].id);
            // this is a child to someone
            if (children_left != 0)
                children_left--;
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            index++;
        } while (children_left > 0);
        
        return index;
    }
    
    void transfer_backward(stack_allocator& from, stack_allocator& to, detail::op_iter begin, detail::op_iter end)
    {
        for (auto it = begin; it != end; it--)
        {
            if (it->is_value)
                from.transfer_bytes(to, it->type_size);
        }
    }
    
    void transfer_forward(stack_allocator& from, stack_allocator& to, detail::op_iter begin, detail::op_iter end)
    {
        // now copy back into the respective children
        for (auto it = begin; it != end; it++)
        {
            if (it->is_value)
                from.transfer_bytes(to, it->type_size);
        }
    }
}