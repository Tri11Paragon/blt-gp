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
    blt::expected<crossover_t::result_t, crossover_t::error_t> crossover_t::apply(gp_program& program, const tree_t& p1, const tree_t& p2) // NOLINT
    {
        const auto& config = program.get_config();
        result_t result{p1, p2};
        
        BLT_INFO("Child 1 Stack empty? %s", result.child1.get_values().empty() ? "true" : "false");
        BLT_INFO("Child 2 Stack empty? %s", result.child2.get_values().empty() ? "true" : "false");
        
        auto& c1 = result.child1;
        auto& c2 = result.child2;
        
        auto& c1_ops = c1.get_operations();
        auto& c2_ops = c2.get_operations();
        
        if (c1_ops.size() < 5 || c2_ops.size() < 5)
            return blt::unexpected(error_t::TREE_TOO_SMALL);
        
        std::uniform_int_distribution op_sel1(3ul, c1_ops.size() - 1);
        std::uniform_int_distribution op_sel2(3ul, c2_ops.size() - 1);
        
        blt::size_t crossover_point = op_sel1(program.get_random());
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
                    for (auto i = attempted_point + 1; i < c2_ops.size(); i++)
                    {
                        auto* info = &program.get_operator_info(c2_ops[i].id);
                        if (info->return_type == crossover_point_type.return_type)
                        {
                            attempted_point = i;
                            attempted_point_type = info;
                            break;
                        }
                    }
                }
                // should we try again over the whole tree? probably not.
                return blt::unexpected(error_t::NO_VALID_TYPE);
            } else
            {
                attempted_point = op_sel2(program.get_random());
                attempted_point_type = &program.get_operator_info(c2_ops[attempted_point].id);
                counter++;
            }
        } while (crossover_point_type.return_type != attempted_point_type->return_type);
        
        blt::i64 children_left = 0;
        blt::size_t index = crossover_point;
        
        do
        {
            const auto& type = program.get_operator_info(c1_ops[index].id);
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            else
                children_left--;
            index++;
        } while (children_left > 0);
        
        auto crossover_point_begin_itr = c1_ops.begin() + static_cast<blt::ptrdiff_t>(crossover_point);
        auto crossover_point_end_itr = c1_ops.begin() + static_cast<blt::ptrdiff_t>(index);
        
        children_left = 0;
        index = attempted_point;
        
        do
        {
            const auto& type = program.get_operator_info(c2_ops[index].id);
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            else
                children_left--;
            if (children_left > 0)
                index++;
            else
                break;
        } while (true);
        
        auto found_point_begin_itr = c2_ops.begin() + static_cast<blt::ptrdiff_t>(attempted_point);
        auto found_point_end_itr = c2_ops.begin() + static_cast<blt::ptrdiff_t>(index);
        
        stack_allocator& c1_stack_init = c1.get_values();
        stack_allocator& c2_stack_init = c2.get_values();
        
        std::vector<op_container_t> c1_operators;
        std::vector<op_container_t> c2_operators;
        
        for (const auto& op : blt::enumerate(crossover_point_begin_itr, crossover_point_end_itr))
            c1_operators.push_back(op);
        for (const auto& op : blt::enumerate(found_point_begin_itr, found_point_end_itr))
            c2_operators.push_back(op);
        
        stack_allocator c1_stack_after_copy;
        stack_allocator c1_stack_for_copy;
        stack_allocator c2_stack_after_copy;
        stack_allocator c2_stack_for_copy;
        
        // transfer all values after the crossover point. these will need to be transferred back to child2
        for (auto it = c1_ops.end() - 1; it != crossover_point_end_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c1_stack_after_copy, c1_stack_init, -1);
        }
        
        // transfer all values for the crossover point.
        for (auto it = crossover_point_end_itr - 1; it != crossover_point_begin_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c1_stack_for_copy, c1_stack_init, -1);
        }
        
        // transfer child2 values for copying back into c1
        for (auto it = c2_ops.end() - 1; it != found_point_end_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c2_stack_after_copy, c2_stack_init, -1);
        }
        
        for (auto it = found_point_end_itr - 1; it != found_point_begin_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c2_stack_for_copy, c2_stack_init, -1);
        }
        
        BLT_TRACE("c1_stack_after_copy empty? %s", c1_stack_after_copy.empty() ? "true" : "false");
        BLT_TRACE("c1_stack_for_copy empty? %s", c1_stack_for_copy.empty() ? "true" : "false");
        BLT_TRACE("c2_stack_after_copy empty? %s", c2_stack_after_copy.empty() ? "true" : "false");
        BLT_TRACE("c2_stack_for_copy empty? %s", c2_stack_for_copy.empty() ? "true" : "false");
        BLT_INFO("Child 1 Stack empty? %s", result.child1.get_values().empty() ? "true" : "false");
        BLT_INFO("Child 2 Stack empty? %s", result.child2.get_values().empty() ? "true" : "false");
        
        // now copy back into the respective children
        for (auto it = found_point_begin_itr; it != found_point_end_itr; it++)
        {
            if (it->is_value)
                it->transfer(c1.get_values(), c2_stack_for_copy, -1);
        }
        
        for (auto it = crossover_point_begin_itr; it != crossover_point_end_itr; it++)
        {
            if (it->is_value)
                it->transfer(c2.get_values(), c1_stack_for_copy, -1);
        }
        
        BLT_TRACE("c1_stack_after_copy empty? %s", c1_stack_after_copy.empty() ? "true" : "false");
        BLT_TRACE("c1_stack_for_copy empty? %s", c1_stack_for_copy.empty() ? "true" : "false");
        BLT_TRACE("c2_stack_after_copy empty? %s", c2_stack_after_copy.empty() ? "true" : "false");
        BLT_TRACE("c2_stack_for_copy empty? %s", c2_stack_for_copy.empty() ? "true" : "false");
        BLT_INFO("Child 1 Stack empty? %s", result.child1.get_values().empty() ? "true" : "false");
        BLT_INFO("Child 2 Stack empty? %s", result.child2.get_values().empty() ? "true" : "false");
        
        // now copy after the crossover point back to the correct children
        for (auto it = crossover_point_end_itr; it != c1_ops.end(); it++)
        {
            if (it->is_value)
                it->transfer(c1.get_values(), c1_stack_after_copy, -1);
        }
        
        for (auto it = found_point_end_itr; it != c2_ops.end(); it++)
        {
            if (it->is_value)
                it->transfer(c2.get_values(), c2_stack_after_copy, -1);
        }
        
        BLT_TRACE("c1_stack_after_copy empty? %s", c1_stack_after_copy.empty() ? "true" : "false");
        BLT_TRACE("c1_stack_for_copy empty? %s", c1_stack_for_copy.empty() ? "true" : "false");
        BLT_TRACE("c2_stack_after_copy empty? %s", c2_stack_after_copy.empty() ? "true" : "false");
        BLT_TRACE("c2_stack_for_copy empty? %s", c2_stack_for_copy.empty() ? "true" : "false");
        
        // now swap the operators
        auto insert_point_c1 = crossover_point_begin_itr - 1;
        auto insert_point_c2 = found_point_begin_itr - 1;
        
        // invalidates [begin, end()) so the insert points should be fine
        c1_ops.erase(crossover_point_begin_itr, crossover_point_end_itr);
        c2_ops.erase(found_point_begin_itr, found_point_end_itr);
        
        c1_ops.insert(++insert_point_c1, c2_operators.begin(), c2_operators.end());
        c1_ops.insert(++insert_point_c2, c1_operators.begin(), c1_operators.end());
        
        BLT_INFO("Child 1 Stack empty? %s", result.child1.get_values().empty() ? "true" : "false");
        BLT_INFO("Child 2 Stack empty? %s", result.child2.get_values().empty() ? "true" : "false");
        
        return result;
    }
}