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
#include <blt/std/utility.h>
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
        transfer_backward(c1_stack_init, c1_stack_after_copy, c1_ops.end() - 1, crossover_point_end_itr - 1);
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

#if BLT_DEBUG_LEVEL >= 2
        blt::size_t c1_found_bytes = result.child1.get_values().size().total_used_bytes;
        blt::size_t c2_found_bytes = result.child2.get_values().size().total_used_bytes;
        blt::size_t c1_expected_bytes = std::accumulate(result.child1.get_operations().begin(), result.child1.get_operations().end(), 0ul,
                                                        [](const auto& v1, const auto& v2) {
                                                            if (v2.is_value)
                                                                return v1 + stack_allocator::aligned_size(v2.type_size);
                                                            return v1;
                                                        });
        blt::size_t c2_expected_bytes = std::accumulate(result.child2.get_operations().begin(), result.child2.get_operations().end(), 0ul,
                                                        [](const auto& v1, const auto& v2) {
                                                            if (v2.is_value)
                                                                return v1 + stack_allocator::aligned_size(v2.type_size);
                                                            return v1;
                                                        });
        if (c1_found_bytes != c1_expected_bytes || c2_found_bytes != c2_expected_bytes)
        {
            BLT_WARN("C1 Found bytes %ld vs Expected Bytes %ld", c1_found_bytes, c1_expected_bytes);
            BLT_WARN("C2 Found bytes %ld vs Expected Bytes %ld", c2_found_bytes, c2_expected_bytes);
            BLT_ABORT("Amount of bytes in stack doesn't match the number of bytes expected for the operations");
        }
#endif
        
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
        
        auto& ops_r = c.get_operations();
        auto& vals_r = c.get_values();
        
        auto begin_point = static_cast<blt::ptrdiff_t>(program.get_random().get_size_t(0ul, ops_r.size()));
        auto end_point = find_endpoint(program, ops_r, begin_point);
        auto begin_operator_id = ops_r[begin_point].id;
        const auto& type_info = program.get_operator_info(begin_operator_id);
        
        auto begin_itr = ops_r.begin() + begin_point;
        auto end_itr = ops_r.begin() + end_point;
        
        auto new_tree = config.generator.get().generate({program, type_info.return_type, config.replacement_min_depth, config.replacement_max_depth});
        
        auto& new_ops_r = new_tree.get_operations();
        auto& new_vals_r = new_tree.get_values();
        
        stack_allocator stack_after;
        //stack_allocator new_vals_flip; // this is annoying.
        transfer_backward(vals_r, stack_after, ops_r.end() - 1, end_itr - 1);
        for (auto it = end_itr - 1; it != begin_itr - 1; it--)
        {
            if (it->is_value)
                vals_r.pop_bytes(static_cast<blt::ptrdiff_t>(stack_allocator::aligned_size(it->type_size)));
        }
        
        vals_r.insert(std::move(new_vals_r));
        transfer_forward(stack_after, vals_r, end_itr, ops_r.end());
        
        auto before = begin_itr - 1;
        ops_r.erase(begin_itr, end_itr);
        ops_r.insert(++before, new_ops_r.begin(), new_ops_r.end());

#if BLT_DEBUG_LEVEL >= 2
        BLT_ASSERT(new_vals_r.empty());
        BLT_ASSERT(stack_after.empty());
        blt::size_t bytes_expected = 0;
        auto bytes_size = vals_r.size().total_used_bytes;
        
        for (const auto& op : c.get_operations())
        {
            if (op.is_value)
                bytes_expected += stack_allocator::aligned_size(op.type_size);
        }
        
        if (bytes_expected != bytes_size)
        {
            BLT_WARN_STREAM << "Stack state: " << vals_r.size() << "\n";
            BLT_WARN("Child tree bytes %ld vs expected %ld, difference: %ld", bytes_size, bytes_expected,
                     static_cast<blt::ptrdiff_t>(bytes_expected) - static_cast<blt::ptrdiff_t>(bytes_size));
            BLT_ABORT("Amount of bytes in stack doesn't match the number of bytes expected for the operations");
        }
        auto copy = c;
        try
        {
            auto result = copy.evaluate(nullptr);
            blt::black_box(result);
        } catch (...)
        {
            std::cout << "This occurred at point " << begin_point << " ending at (old) " << end_point << "\n";
            std::cout << "our root type is " << ops_r[begin_point].id << " with size " << stack_allocator::aligned_size(ops_r[begin_point].type_size)
                      << "\n";
            std::cout << "now Named: " << (program.get_name(ops_r[begin_point].id) ? *program.get_name(ops_r[begin_point].id) : "Unnamed") << "\n";
            std::cout << "Was named: " << (program.get_name(begin_operator_id) ? *program.get_name(begin_operator_id) : "Unnamed") << "\n";
            std::cout << "Parent:" << std::endl;
            p.print(program, std::cout, false, true);
            std::cout << "Child:" << std::endl;
            c.print(program, std::cout, false, true);
            std::cout << std::endl;
            c.print(program, std::cout, true, true);
            std::cout << std::endl;
            throw std::exception();
        }
#endif
        
        return c;
    }

//    tree_t mutation_t::apply(gp_program& program, const tree_t& p)
//    {
//        auto c = p;
//
//#if BLT_DEBUG_LEVEL >= 2
//        blt::size_t parent_bytes = 0;
//        blt::size_t parent_size = p.get_values().size().total_used_bytes;
//        for (const auto& op : p.get_operations())
//        {
//            if (op.is_value)
//                parent_bytes += stack_allocator::aligned_size(op.type_size);
//        }
//        if (parent_bytes != parent_size)
//        {
//            BLT_WARN("Parent bytes %ld do not match expected %ld", parent_size, parent_bytes);
//            BLT_ABORT("You should not ignore the mismatched parent bytes!");
//        }
//#endif
//
//        auto& ops = c.get_operations();
//        auto& vals = c.get_values();
//
//        auto point = static_cast<blt::ptrdiff_t>(program.get_random().get_size_t(0ul, ops.size()));
//        const auto& type_info = program.get_operator_info(ops[point].id);
//
//        auto new_tree = config.generator.get().generate({program, type_info.return_type, config.replacement_min_depth, config.replacement_max_depth});
//
//        auto& new_ops = new_tree.get_operations();
//        auto& new_vals = new_tree.get_values();
//
//#if BLT_DEBUG_LEVEL >= 2
//        blt::size_t new_tree_bytes = 0;
//        blt::size_t new_tree_size = new_vals.size().total_used_bytes;
//        for (const auto& op : new_ops)
//        {
//            if (op.is_value)
//                new_tree_bytes += stack_allocator::aligned_size(op.type_size);
//        }
//#endif
//
//        auto begin_p = ops.begin() + point;
//        auto end_p = ops.begin() + find_endpoint(program, ops, point);
//
//        stack_allocator after_stack;
//
//#if BLT_DEBUG_LEVEL >= 2
//        blt::size_t after_stack_bytes = 0;
//        blt::size_t for_bytes = 0;
//        for (auto it = ops.end() - 1; it != end_p - 1; it--)
//        {
//            if (it->is_value)
//            {
//                after_stack_bytes += stack_allocator::aligned_size(it->type_size);
//            }
//        }
//#endif
//
//        transfer_backward(vals, after_stack, ops.end() - 1, end_p - 1);
//        //for (auto it = ops.end() - 1; it != end_p; it++)
//
//        for (auto it = end_p - 1; it != begin_p - 1; it--)
//        {
//            if (it->is_value)
//            {
//#if BLT_DEBUG_LEVEL >= 2
//                auto size_b = vals.size().total_used_bytes;
//#endif
//                vals.pop_bytes(static_cast<blt::ptrdiff_t>(stack_allocator::aligned_size(it->type_size)));
//#if BLT_DEBUG_LEVEL >= 2
//                auto size_a = vals.size().total_used_bytes;
//                if (size_a != size_b - stack_allocator::aligned_size(it->type_size))
//                {
//                    BLT_WARN("After pop size: %ld before pop size: %ld; expected pop amount %ld", size_a, size_b,
//                             stack_allocator::aligned_size(it->type_size));
//                    BLT_ABORT("Popping bytes didn't remove the correct amount!");
//                }
//                for_bytes += stack_allocator::aligned_size(it->type_size);
//#endif
//            }
//        }
//
//        transfer_backward(new_vals, vals, new_ops.end() - 1, new_ops.begin() - 1);
//
//        transfer_forward(after_stack, vals, end_p, ops.end());
//
//        auto before = begin_p - 1;
//        ops.erase(begin_p, end_p);
//        ops.insert(++before, new_ops.begin(), new_ops.end());
//
//#if BLT_DEBUG_LEVEL >= 2
//        blt::size_t bytes_expected = 0;
//        auto bytes_size = c.get_values().size().total_used_bytes;
//
//        for (const auto& op : c.get_operations())
//        {
//            if (op.is_value)
//                bytes_expected += stack_allocator::aligned_size(op.type_size);
//        }
//
//        if (bytes_expected != bytes_size || parent_size != parent_bytes || new_tree_size != new_tree_bytes)
//        {
//            BLT_WARN("Parent bytes %ld vs expected %ld", parent_size, parent_bytes);
//            BLT_WARN("After stack bytes: %ld; popped bytes %ld", after_stack_bytes, for_bytes);
//            BLT_WARN("Tree bytes %ld vs expected %ld", new_tree_size, new_tree_bytes);
//            BLT_WARN("Child tree bytes %ld vs expected %ld", bytes_size, bytes_expected);
//            BLT_ABORT("Amount of bytes in stack doesn't match the number of bytes expected for the operations");
//        }
//        auto copy = c;
//        try
//        {
//            auto result = copy.evaluate(nullptr);
//            blt::black_box(result);
//        } catch(...) {
//            std::cout << "Parent:\n";
//            p.print(program, std::cout, false, true);
//            std::cout << "Child:\n";
//            c.print(program, std::cout, false, true);
//            c.print(program, std::cout, true, true);
//            throw std::exception();
//        }
//
//#endif
//
//        return c;
//    }
    
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