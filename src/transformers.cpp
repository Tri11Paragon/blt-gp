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
        result_t result{p1, p2};

#if BLT_DEBUG_LEVEL > 0
        BLT_INFO("Child 1 Stack empty? %s", result.child1.get_values().empty() ? "true" : "false");
        BLT_INFO("Child 2 Stack empty? %s", result.child2.get_values().empty() ? "true" : "false");
#endif
        
        auto& c1 = result.child1;
        auto& c2 = result.child2;
        
        auto& c1_ops = c1.get_operations();
        auto& c2_ops = c2.get_operations();
        
        if (c1_ops.size() < 5 || c2_ops.size() < 5)
            return blt::unexpected(error_t::TREE_TOO_SMALL);
        
        std::uniform_int_distribution op_sel1(3ul, c1_ops.size() - 1);
        std::uniform_int_distribution op_sel2(3ul, c2_ops.size() - 1);
        
        blt::size_t crossover_point = op_sel1(program.get_random());
        
        while (config.avoid_terminals && program.get_operator_info(c1_ops[crossover_point].id).argc.is_terminal())
            crossover_point = op_sel1(program.get_random());
        
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
                attempted_point = op_sel2(program.get_random());
                attempted_point_type = &program.get_operator_info(c2_ops[attempted_point].id);
                if (config.avoid_terminals && attempted_point_type->argc.is_terminal())
                    continue;
                if (crossover_point_type.return_type == attempted_point_type->return_type)
                    break;
                counter++;
            }
        } while (true);
        
        blt::i64 children_left = 0;
        blt::size_t index = crossover_point;
        
        do
        {
            const auto& type = program.get_operator_info(c1_ops[index].id);
#if BLT_DEBUG_LEVEL > 1
    #define MAKE_C_STR() program.get_name(c1_ops[index].id).has_value() ? std::string(program.get_name(c1_ops[index].id).value()).c_str() : std::to_string(c1_ops[index].id).c_str()
            BLT_TRACE("Crossover type: %s, op: %s", std::string(program.get_typesystem().get_type(type.return_type).name()).c_str(), MAKE_C_STR());
    #undef MAKE_C_STR
#endif
            // this is a child to someone
            if (children_left != 0)
                children_left--;
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            index++;
        } while (children_left > 0);
        
        auto crossover_point_begin_itr = c1_ops.begin() + static_cast<blt::ptrdiff_t>(crossover_point);
        auto crossover_point_end_itr = c1_ops.begin() + static_cast<blt::ptrdiff_t>(index);
#if BLT_DEBUG_LEVEL > 0
        BLT_TRACE("[%ld %ld) %ld", crossover_point, index, index - crossover_point);
#endif
        
        children_left = 0;
        index = attempted_point;
        
        do
        {
            const auto& type = program.get_operator_info(c2_ops[index].id);
#if BLT_DEBUG_LEVEL > 1
    #define MAKE_C_STR() program.get_name(c2_ops[index].id).has_value() ? std::string(program.get_name(c2_ops[index].id).value()).c_str() : std::to_string(c2_ops[index].id).c_str()
            BLT_TRACE("Found type: %s, op: %s", std::string(program.get_typesystem().get_type(type.return_type).name()).c_str(), MAKE_C_STR());
    #undef MAKE_C_STR
#endif
            // this is a child to someone
            if (children_left != 0)
                children_left--;
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            
            index++;
        } while (children_left > 0);
        
        auto found_point_begin_itr = c2_ops.begin() + static_cast<blt::ptrdiff_t>(attempted_point);
        auto found_point_end_itr = c2_ops.begin() + static_cast<blt::ptrdiff_t>(index);

#if BLT_DEBUG_LEVEL > 0
        BLT_TRACE("[%ld %ld) %ld", attempted_point, index, index - attempted_point);
#endif
        
        stack_allocator& c1_stack_init = c1.get_values();
        stack_allocator& c2_stack_init = c2.get_values();
        
        std::vector<op_container_t> c1_operators;
        std::vector<op_container_t> c2_operators;
        
        for (const auto& op : blt::enumerate(crossover_point_begin_itr, crossover_point_end_itr))
            c1_operators.push_back(op);
        for (const auto& op : blt::enumerate(found_point_begin_itr, found_point_end_itr))
            c2_operators.push_back(op);

#if BLT_DEBUG_LEVEL > 0
        BLT_TRACE("Sizes: %ld %ld || Ops size: %ld %ld", c1_operators.size(), c2_operators.size(), c1_ops.size(), c2_ops.size());
#endif
        
        stack_allocator c1_stack_after_copy;
        stack_allocator c1_stack_for_copy;
        stack_allocator c2_stack_after_copy;
        stack_allocator c2_stack_for_copy;

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring past crossover 1:");
#endif
        // transfer all values after the crossover point. these will need to be transferred back to child2
        for (auto it = c1_ops.end() - 1; it != crossover_point_end_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c1_stack_after_copy, c1_stack_init);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring for crossover 1:");
#endif
        // transfer all values for the crossover point.
        for (auto it = crossover_point_end_itr - 1; it != crossover_point_begin_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c1_stack_for_copy, c1_stack_init);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring past crossover 2:");
#endif
        // transfer child2 values for copying back into c1
        for (auto it = c2_ops.end() - 1; it != found_point_end_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c2_stack_after_copy, c2_stack_init);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring for crossover 2:");
#endif
        for (auto it = found_point_end_itr - 1; it != found_point_begin_itr - 1; it--)
        {
            if (it->is_value)
                it->transfer(c2_stack_for_copy, c2_stack_init);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring back for crossover 1:");
#endif
        // now copy back into the respective children
        for (auto it = found_point_begin_itr; it != found_point_end_itr; it++)
        {
            if (it->is_value)
                it->transfer(c1.get_values(), c2_stack_for_copy);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring back for crossover 2:");
#endif
        for (auto it = crossover_point_begin_itr; it != crossover_point_end_itr; it++)
        {
            if (it->is_value)
                it->transfer(c2.get_values(), c1_stack_for_copy);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring back after crossover 1:");
#endif
        // now copy after the crossover point back to the correct children
        for (auto it = crossover_point_end_itr; it != c1_ops.end(); it++)
        {
            if (it->is_value)
                it->transfer(c1.get_values(), c1_stack_after_copy);
        }

#if BLT_DEBUG_LEVEL > 1
        BLT_DEBUG("Transferring back after crossover 2:");
#endif
        for (auto it = found_point_end_itr; it != c2_ops.end(); it++)
        {
            if (it->is_value)
                it->transfer(c2.get_values(), c2_stack_after_copy);
        }
        
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
    
    tree_t mutation_t::apply(gp_program& program, tree_generator_t& generator, const tree_t& p)
    {
        auto c = p;
        
        auto& ops = c.get_operations();
        auto& vals = c.get_values();
        
        std::uniform_int_distribution point_sel_dist(0ul, ops.size() - 1);
        auto point = point_sel_dist(program.get_random());
        const auto& type_info = program.get_operator_info(ops[point].id);
        
        blt::i64 children_left = 0;
        blt::size_t index = point;
        
        do
        {
            const auto& type = program.get_operator_info(ops[index].id);
            
            // this is a child to someone
            if (children_left != 0)
                children_left--;
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            index++;
        } while (children_left > 0);
        
        auto begin_p = ops.begin() + static_cast<blt::ptrdiff_t>(point);
        auto end_p = ops.begin() + static_cast<blt::ptrdiff_t>(index);
        
        stack_allocator after_stack;
        //std::vector<op_container_t> after_ops;
        
        for (auto it = ops.end() - 1; it != end_p - 1; it--)
        {
            if (it->is_value)
            {
                it->transfer(after_stack, vals);
                //after_ops.push_back(*it);
            }
        }
        
        for (auto it = end_p - 1; it != begin_p - 1; it--)
        {
            if (it->is_value)
                it->transfer(std::optional<std::reference_wrapper<stack_allocator>>{}, vals);
        }
        
        auto before = begin_p - 1;
        
        ops.erase(begin_p, end_p);
        
        auto new_tree = generator.generate({program, type_info.return_type, config.replacement_min_depth, config.replacement_max_depth});
        
        auto& new_ops = new_tree.get_operations();
        auto& new_vals = new_tree.get_values();
        
        ops.insert(++before, new_ops.begin(), new_ops.end());
        
        for (const auto& op : new_ops)
        {
            if (op.is_value)
                op.transfer(vals, new_vals);
        }
        
        auto new_end_point = point + new_ops.size();
        auto new_end_p = ops.begin() + static_cast<blt::ptrdiff_t>(new_end_point);
        
        for (auto it = new_end_p; it != ops.end(); it++)
        {
            if (it->is_value)
                it->transfer(vals, after_stack);
        }
        
        return c;
    }
}