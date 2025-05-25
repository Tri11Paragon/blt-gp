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
#include <blt/gp/tree.h>
#include <blt/gp/stack.h>
#include <blt/std/assert.h>
#include <blt/logging/logging.h>
#include <blt/gp/program.h>
#include <stack>
#include <utility>

namespace blt::gp
{
    struct buffer_wrapper_t
    {
        expanding_buffer<u8> buffer;

        u8* get(const size_t size)
        {
            if (size > buffer.size())
                buffer.resize(size);
            return buffer.data();
        }
    };

    struct bool_flag_t
    {
        enum state_t : u8
        {
            FALSE = 0,
            TRUE = 1,
            TRUE_BUT_REMOVE = 2
        };

        bool_flag_t() = default;

        explicit bool_flag_t(const bool value): value(value ? 1 : 0)
        {
        }

        explicit bool_flag_t(const u8 value): value(value)
        {
        }

        bool_flag_t(const bool_flag_t& other) = default;
        bool_flag_t& operator=(const bool_flag_t& other) = default;

        bool_flag_t& operator=(const bool b)
        {
            value = b ? 1 : 0;
            return *this;
        }

        bool_flag_t& operator=(const u8 v)
        {
            value = v;
            return *this;
        }

        bool_flag_t& operator=(const state_t s)
        {
            value = static_cast<u8>(s);
            return *this;
        }

        [[nodiscard]] explicit operator bool() const
        {
            return value != 0;
        }

        [[nodiscard]] bool should_remove() const
        {
            return value == TRUE_BUT_REMOVE;
        }

        u8 value = 0;
    };

    void print_child(child_t child)
    {
        BLT_TRACE("\tChild: {} -> {} ( {} )", child.start, child.end, child.size());
    }

    void print_child_vec(const std::vector<child_t>& children)
    {
        BLT_TRACE("Children: ");
        for (const auto& c : children)
            print_child(c);
    }

    std::ostream& create_indent(std::ostream& out, blt::size_t amount, bool pretty_print)
    {
        if (!pretty_print)
            return out;
        for (blt::size_t i = 0; i < amount; i++)
            out << '\t';
        return out;
    }

    std::string_view end_indent(bool pretty_print)
    {
        return pretty_print ? "\n" : "";
    }

    std::string get_return_type(gp_program& program, type_id id, bool use_returns)
    {
        if (!use_returns)
            return "";
        return "(" + std::string(program.get_typesystem().get_type(id).name()) + ")";
    }

    void tree_t::byte_only_transaction_t::move(const size_t bytes_to_move)
    {
        bytes = bytes_to_move;
        thread_local buffer_wrapper_t buffer;
        data = buffer.get(bytes);
        tree.values.copy_to(data, bytes);
        tree.values.pop_bytes(bytes);
    }

    void tree_t::print(std::ostream& out, const bool print_literals, const bool pretty_print, const bool include_types,
                       const ptrdiff_t marked_index) const
    {
        std::stack<blt::size_t> arguments_left;
        blt::size_t indent = 0;

        stack_allocator reversed;
        if (print_literals)
        {
            // I hate this.
            stack_allocator copy = values;

            // reverse the order of the stack
            for (const auto& v : operations)
            {
                if (v.is_value())
                    copy.transfer_bytes(reversed, v.type_size());
            }
        }
        for (const auto& [i, v] : enumerate(operations))
        {
            auto info = m_program->get_operator_info(v.id());
            const auto name = m_program->get_name(v.id()) ? m_program->get_name(v.id()).value() : "NULL";
            auto return_type = get_return_type(*m_program, info.return_type, include_types);
            if (static_cast<ptrdiff_t>(i) == marked_index)
            {
                out << "[ERROR OCCURRED HERE] -> ";
            }
            if (info.argc.argc > 0)
            {
                create_indent(out, indent, pretty_print) << "(";
                indent++;
                arguments_left.emplace(info.argc.argc);
                out << name << return_type << end_indent(pretty_print);
            }
            else
            {
                if (print_literals)
                {
                    create_indent(out, indent, pretty_print);
                    if (m_program->is_operator_ephemeral(v.id()))
                    {
                        m_program->get_print_func(v.id())(out, reversed);
                        reversed.pop_bytes(v.type_size());
                    }
                    else
                        out << name;
                    out << return_type << end_indent(pretty_print);
                }
                else
                    create_indent(out, indent, pretty_print) << name << return_type << end_indent(pretty_print);
            }

            while (!arguments_left.empty())
            {
                const auto top = arguments_left.top();
                arguments_left.pop();
                if (top == 0)
                {
                    indent--;
                    create_indent(out, indent, pretty_print) << ")" << end_indent(pretty_print);
                    continue;
                }
                if (!pretty_print)
                    out << " ";
                arguments_left.push(top - 1);
                break;
            }
        }
        while (!arguments_left.empty())
        {
            const auto top = arguments_left.top();
            arguments_left.pop();
            if (top == 0)
            {
                indent--;
                create_indent(out, indent, pretty_print) << ")" << end_indent(pretty_print);
                continue;
            }
            else
            {
                BLT_ERROR("Failed to print tree correctly!");
                break;
            }
        }

        out << '\n';
    }

    size_t tree_t::get_depth(gp_program& program) const
    {
        size_t depth = 0;

        auto operations_stack = operations;
        thread_local tracked_vector<size_t> values_process;
        thread_local tracked_vector<size_t> value_stack;

        values_process.clear();
        value_stack.clear();

        for (const auto& op : operations_stack)
        {
            if (op.is_value())
                value_stack.push_back(1);
        }

        while (!operations_stack.empty())
        {
            auto operation = operations_stack.back();
            // keep the last value in the stack on the process stack stored in the eval context, this way it can be accessed easily.
            operations_stack.pop_back();
            if (operation.is_value())
            {
                auto d = value_stack.back();
                depth = std::max(depth, d);
                values_process.push_back(d);
                value_stack.pop_back();
                continue;
            }
            size_t local_depth = 0;
            for (size_t i = 0; i < program.get_operator_info(operation.id()).argc.argc; i++)
            {
                local_depth = std::max(local_depth, values_process.back());
                values_process.pop_back();
            }
            value_stack.push_back(local_depth + 1);
            operations_stack.emplace_back(operation.type_size(), operation.id(), true, program.get_operator_flags(operation.id()));
        }

        return depth;
    }

    subtree_point_t tree_t::select_subtree(const double terminal_chance) const
    {
        do
        {
            const auto point = m_program->get_random().get_u64(0, operations.size());
            const auto& info = m_program->get_operator_info(operations[point].id());
            if (!info.argc.is_terminal())
                return {static_cast<ptrdiff_t>(point), info};
            if (m_program->get_random().choice(terminal_chance))
                return {static_cast<ptrdiff_t>(point), info};
        }
        while (true);
    }

    std::optional<subtree_point_t> tree_t::select_subtree(const type_id type, const u32 max_tries, const double terminal_chance) const
    {
        for (u32 i = 0; i < max_tries; ++i)
        {
            if (const auto tree = select_subtree(terminal_chance); tree.get_type() == type)
                return tree;
        }
        return {};
    }

    subtree_point_t tree_t::select_subtree_traverse(const double terminal_chance, const double depth_multiplier) const
    {
        size_t index = 0;
        double depth = 0;
        double exit_chance = 0;
        while (true)
        {
            const auto& info = m_program->get_operator_info(operations[index].id());
            if (info.argc.is_terminal())
            {
                if (m_program->get_random().choice(terminal_chance))
                    return {static_cast<ptrdiff_t>(index), info};
                index = 0;
                depth = 0;
                exit_chance = 0;
                continue;
            }
            if (m_program->get_random().choice(exit_chance))
                return {static_cast<ptrdiff_t>(index), info};

            const auto child = m_program->get_random().get_u32(0, info.argc.argc);
            index++;
            for (u32 i = 0; i < child; i++)
                index = find_endpoint(static_cast<ptrdiff_t>(index));

            ++depth;
            exit_chance = 1.0 - (1.0 / (1 + depth * depth_multiplier * 0.5));
        }
    }

    std::optional<subtree_point_t> tree_t::select_subtree_traverse(const type_id type, const u32 max_tries, const double terminal_chance,
                                                                   const double depth_multiplier) const
    {
        for (u32 i = 0; i < max_tries; ++i)
        {
            if (const auto tree = select_subtree_traverse(terminal_chance, depth_multiplier); tree.get_type() == type)
                return tree;
        }
        return {};
    }

    void slow_tree_manipulator_t::copy_subtree(const subtree_point_t point, const ptrdiff_t extent, tracked_vector<op_container_t>& operators,
                                               stack_allocator& stack) const
    {
        const auto point_begin_itr = tree->operations.begin() + point.get_spoint();
        const auto point_end_itr = tree->operations.begin() + extent;

        const size_t after_bytes = calculate_ephemeral_size(point_end_itr, tree->operations.end());

        const size_t ops = std::distance(point_begin_itr, point_end_itr);
        operators.reserve(operators.size() + ops);
        // TODO something better!
        for (size_t i = 0; i < ops; ++i)
            operators.emplace_back(0, 0, false, operator_special_flags{});
        size_t for_bytes = 0;
        size_t pos = 0;
        for (auto& it : iterate(point_begin_itr, point_end_itr).rev())
        {
            if (it.is_value())
            {
                for_bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto [_, ptr] = tree->values.access_pointer(for_bytes + after_bytes, it.type_size());
                    ++*ptr;
                }
            }
            operators[operators.size() - 1 - (pos++)] = it;
        }

        stack.copy_from(tree->values, for_bytes, after_bytes);
    }

    void slow_tree_manipulator_t::copy_subtree(const subtree_point_t point, tracked_vector<op_container_t>& operators, stack_allocator& stack) const
    {
        copy_subtree(point, tree->find_endpoint(point.get_spoint()), operators, stack);
    }

    void slow_tree_manipulator_t::copy_subtree(const subtree_point_t point, const ptrdiff_t extent, tree_t& out_tree) const
    {
        copy_subtree(point, extent, out_tree.operations, out_tree.values);
    }

    void slow_tree_manipulator_t::copy_subtree(const subtree_point_t point, tree_t& out_tree) const
    {
        copy_subtree(point, tree->find_endpoint(point.get_spoint()), out_tree);
    }

    void slow_tree_manipulator_t::copy_subtree(const child_t subtree, tree_t& out_tree) const
    {
        copy_subtree(subtree_point_t{subtree.start}, subtree.end, out_tree);
    }

    void slow_tree_manipulator_t::swap_subtrees(const child_t subtree1, const child_t subtree2) const
    {
        const auto last_end = std::max(subtree1.end, subtree2.end);
        const auto first_end = std::min(subtree1.end, subtree2.end);
        const auto first_begin = std::min(subtree1.start, subtree2.start);
        const auto last_begin = std::max(subtree1.start, subtree2.start);

        const auto last_end_itr = tree->operations.begin() + last_end;
        const auto first_end_itr = tree->operations.begin() + first_end;
        const auto first_begin_itr = tree->operations.begin() + first_begin;
        const auto last_begin_itr = tree->operations.begin() + last_begin;

        const auto size_first = calculate_ephemeral_size(first_begin_itr, first_end_itr);
        const auto size_between = calculate_ephemeral_size(first_end_itr, last_begin_itr);
        const auto size_last = calculate_ephemeral_size(last_begin_itr, last_end_itr);
        const auto size_after = calculate_ephemeral_size(last_end_itr, tree->operations.end());

        const auto size_total = size_first + size_between + size_last + size_after;

        thread_local struct
        {
            buffer_wrapper_t byte_storage;
            std::vector<op_container_t> ops_storage;
        } storage;
        storage.ops_storage.clear();
        storage.ops_storage.reserve(std::distance(first_begin_itr, first_end_itr));

        const auto ptr = storage.byte_storage.get(size_total);
        tree->values.copy_to(ptr, size_total);
        tree->values.pop_bytes(size_total);

        BLT_TRACE("Removing Bytes! {} = First {} + Between {} + Last {} + After {}", size_total, size_first, size_between, size_last, size_after);

        tree->values.copy_from(ptr + size_first + size_between, size_last);
        tree->values.copy_from(ptr + size_first, size_between);
        tree->values.copy_from(ptr, size_first);
        tree->values.copy_from(ptr + size_total - size_after, size_after);

        const auto distance_between = subtree1.end - subtree2.start;

        storage.ops_storage.insert(storage.ops_storage.end(), first_begin_itr, first_end_itr);
        auto after = tree->operations.erase(first_begin_itr, first_end_itr);
        after = tree->operations.insert(after, after + distance_between, after + distance_between + subtree2.size());
        after += subtree2.size();
        after += distance_between;
        after = tree->operations.erase(after, after + subtree2.size());
        tree->operations.insert(after, storage.ops_storage.begin(), storage.ops_storage.end());
    }

    void slow_tree_manipulator_t::swap_subtrees(const child_t our_subtree, tree_t& other_tree, const child_t other_subtree) const
    {
        const auto c1_subtree_begin_itr = tree->operations.begin() + our_subtree.start;
        const auto c1_subtree_end_itr = tree->operations.begin() + our_subtree.end;

        const auto c2_subtree_begin_itr = other_tree.operations.begin() + other_subtree.start;
        const auto c2_subtree_end_itr = other_tree.operations.begin() + other_subtree.end;

        thread_local struct
        {
            tracked_vector<op_container_t> c1_subtree_operators;
            tracked_vector<op_container_t> c2_subtree_operators;
            buffer_wrapper_t c1_buffer;
            buffer_wrapper_t c2_buffer;
        } storage;
        storage.c1_subtree_operators.clear();
        storage.c2_subtree_operators.clear();

        storage.c1_subtree_operators.reserve(std::distance(c1_subtree_begin_itr, c1_subtree_end_itr));
        storage.c2_subtree_operators.reserve(std::distance(c2_subtree_begin_itr, c2_subtree_end_itr));

        // i don't think this is required for swapping values, since the total number of additions is net zero
        // the tree isn't destroyed at any point.

        size_t c1_subtree_bytes = 0;
        for (const auto& it : iterate(c1_subtree_begin_itr, c1_subtree_end_itr))
        {
            if (it.is_value())
                c1_subtree_bytes += it.type_size();
            storage.c1_subtree_operators.push_back(it);
        }

        size_t c2_subtree_bytes = 0;
        for (const auto& it : iterate(c2_subtree_begin_itr, c2_subtree_end_itr))
        {
            if (it.is_value())
                c2_subtree_bytes += it.type_size();
            storage.c2_subtree_operators.push_back(it);
        }

        const size_t c1_stack_after_bytes = calculate_ephemeral_size(c1_subtree_end_itr, tree->operations.end());
        const size_t c2_stack_after_bytes = calculate_ephemeral_size(c2_subtree_end_itr, other_tree.operations.end());
        const auto c1_total = static_cast<ptrdiff_t>(c1_stack_after_bytes + c1_subtree_bytes);
        const auto c2_total = static_cast<ptrdiff_t>(c2_stack_after_bytes + c2_subtree_bytes);
        const auto copy_ptr_c1 = storage.c1_buffer.get(c1_total);
        const auto copy_ptr_c2 = storage.c2_buffer.get(c2_total);

        tree->values.reserve(tree->values.stored() - c1_subtree_bytes + c2_subtree_bytes);
        other_tree.values.reserve(other_tree.values.stored() - c2_subtree_bytes + c1_subtree_bytes);

        tree->values.copy_to(copy_ptr_c1, c1_total);
        tree->values.pop_bytes(c1_total);

        other_tree.values.copy_to(copy_ptr_c2, c2_total);
        other_tree.values.pop_bytes(c2_total);

        other_tree.values.copy_from(copy_ptr_c1, c1_subtree_bytes);
        other_tree.values.copy_from(copy_ptr_c2 + c2_subtree_bytes, c2_stack_after_bytes);

        tree->values.copy_from(copy_ptr_c2, c2_subtree_bytes);
        tree->values.copy_from(copy_ptr_c1 + c1_subtree_bytes, c1_stack_after_bytes);

        // now swap the operators
        // auto insert_point_c1 = c1_subtree_begin_itr - 1;
        // auto insert_point_c2 = c2_subtree_begin_itr - 1;

        // invalidates [begin, end()) so the insert points should be fine
        const auto insert_point_c1 = tree->operations.erase(c1_subtree_begin_itr, c1_subtree_end_itr);
        const auto insert_point_c2 = other_tree.operations.erase(c2_subtree_begin_itr, c2_subtree_end_itr);

        tree->operations.insert(insert_point_c1, storage.c2_subtree_operators.begin(), storage.c2_subtree_operators.end());
        other_tree.operations.insert(insert_point_c2, storage.c1_subtree_operators.begin(), storage.c1_subtree_operators.end());
    }

    void slow_tree_manipulator_t::swap_subtrees(const subtree_point_t our_subtree, tree_t& other_tree, const subtree_point_t other_subtree) const
    {
        swap_subtrees(child_t{our_subtree.get_spoint(), tree->find_endpoint(our_subtree.get_spoint())}, other_tree,
                      child_t{other_subtree.get_spoint(), other_tree.find_endpoint(other_subtree.get_spoint())});
    }

    void slow_tree_manipulator_t::replace_subtree(const subtree_point_t point, const ptrdiff_t extent, const tree_t& other_tree) const
    {
        const auto point_begin_itr = tree->operations.begin() + point.get_spoint();
        const auto point_end_itr = tree->operations.begin() + extent;

        const size_t after_bytes = calculate_ephemeral_size(point_end_itr, tree->operations.end());

        size_t for_bytes = 0;
        for (auto& it : iterate(point_begin_itr, point_end_itr).rev())
        {
            if (it.is_value())
            {
                for_bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto [val, ptr] = tree->values.access_pointer(for_bytes + after_bytes, it.type_size());
                    --*ptr;
                    if (*ptr == 0)
                        tree->handle_ptr_empty(ptr, val, it.id());
                }
            }
        }
        auto insert = tree->operations.erase(point_begin_itr, point_end_itr);

        thread_local buffer_wrapper_t buffer;
        const auto ptr = buffer.get(after_bytes);
        tree->values.copy_to(ptr, after_bytes);
        tree->values.pop_bytes(after_bytes + for_bytes);

        size_t copy_bytes = 0;
        for (const auto& v : other_tree.operations)
        {
            if (v.is_value())
            {
                if (v.get_flags().is_ephemeral() && v.has_ephemeral_drop())
                {
                    auto [_, pointer] = other_tree.values.access_pointer_forward(copy_bytes, v.type_size());
                    ++*pointer;
                }
                copy_bytes += v.type_size();
            }
            insert = ++tree->operations.emplace(insert, v);
        }

        tree->values.insert(other_tree.values);
        tree->values.copy_from(ptr, after_bytes);
    }

    void slow_tree_manipulator_t::replace_subtree(const subtree_point_t point, const tree_t& other_tree) const
    {
        replace_subtree(point, tree->find_endpoint(point.get_spoint()), other_tree);
    }

    void slow_tree_manipulator_t::delete_subtree(const subtree_point_t point, const ptrdiff_t extent) const
    {
        const auto point_begin_itr = tree->operations.begin() + point.get_spoint();
        const auto point_end_itr = tree->operations.begin() + extent;

        const size_t after_bytes = calculate_ephemeral_size(point_end_itr, tree->operations.end());

        size_t for_bytes = 0;
        for (auto& it : iterate(point_begin_itr, point_end_itr).rev())
        {
            if (it.is_value())
            {
                for_bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto [val, ptr] = tree->values.access_pointer(for_bytes + after_bytes, it.type_size());
                    --*ptr;
                    if (*ptr == 0)
                        tree->handle_ptr_empty(ptr, val, it.id());
                }
            }
        }
        tree->operations.erase(point_begin_itr, point_end_itr);

        thread_local buffer_wrapper_t buffer;
        const auto ptr = buffer.get(after_bytes);
        tree->values.copy_to(ptr, after_bytes);
        tree->values.pop_bytes(after_bytes + for_bytes);
        tree->values.copy_from(ptr, after_bytes);
    }

    void slow_tree_manipulator_t::delete_subtree(const subtree_point_t point) const
    {
        delete_subtree(point, tree->find_endpoint(point.get_spoint()));
    }

    void slow_tree_manipulator_t::delete_subtree(const child_t subtree) const
    {
        delete_subtree(subtree_point_t{subtree.start, tree->m_program->get_operator_info(tree->operations[subtree.start].id())}, subtree.end);
    }

    ptrdiff_t slow_tree_manipulator_t::insert_subtree(const subtree_point_t point, tree_t& other_tree) const
    {
        const size_t after_bytes = calculate_ephemeral_size(tree->operations.begin() + point.get_spoint(), tree->operations.end());
        tree_t::byte_only_transaction_t transaction{*tree, after_bytes};

        auto insert = tree->operations.begin() + point.get_spoint();
        size_t bytes = 0;
        for (auto& it : iterate(other_tree.operations).rev())
        {
            if (it.is_value())
            {
                bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto [_, ptr] = other_tree.values.access_pointer(bytes, it.type_size());
                    ++*ptr;
                }
            }
            insert = tree->operations.insert(insert, it);
        }
        tree->values.insert(other_tree.values);

        return static_cast<ptrdiff_t>(point.get_spoint() + other_tree.size());
    }

    void slow_tree_manipulator_t::modify_operator(const size_t point, operator_id new_id, std::optional<type_id> return_type) const
    {
        if (!return_type)
            return_type = tree->m_program->get_operator_info(new_id).return_type;
        tree_t::byte_only_transaction_t move_data{*tree};
        if (tree->operations[point].is_value())
        {
            const size_t after_bytes = calculate_ephemeral_size(tree->operations.begin() + static_cast<ptrdiff_t>(point) + 1, tree->operations.end());
            move_data.move(after_bytes);
            if (tree->operations[point].get_flags().is_ephemeral() && tree->operations[point].has_ephemeral_drop())
            {
                auto [val, ptr] = tree->values.access_pointer(tree->operations[point].type_size(), tree->operations[point].type_size());
                --*ptr;
                if (*ptr == 0)
                    tree->handle_ptr_empty(ptr, val, tree->operations[point].id());
            }
            tree->values.pop_bytes(tree->operations[point].type_size());
        }
        tree->operations[point] = {
            tree->m_program->get_typesystem().get_type(*return_type).size(),
            new_id,
            tree->m_program->is_operator_ephemeral(new_id),
            tree->m_program->get_operator_flags(new_id)
        };
        if (tree->operations[point].get_flags().is_ephemeral())
        {
            if (move_data.empty())
            {
                const size_t after_bytes = calculate_ephemeral_size(tree->operations.begin() + static_cast<ptrdiff_t>(point) + 1,
                                                                    tree->operations.end());
                move_data.move(after_bytes);
            }
            tree->handle_operator_inserted(tree->operations[point]);
        }
    }

    void slow_tree_manipulator_t::swap_operators(const subtree_point_t point, tree_t& other_tree, const subtree_point_t other_point) const
    {
        const auto point_info = tree->m_program->get_operator_info(tree->operations[point.get_spoint()].id());
        const auto other_point_info = tree->m_program->get_operator_info(other_tree.operations[other_point.get_spoint()].id());

        if (point_info.return_type != other_point_info.return_type)
            return;

        // terminals are special case, since the result ends up being a subtree swap, skip building a complex type table
        if (point_info.argc.argc == 0 || other_point_info.argc.argc == 0)
            return swap_subtrees(point, other_tree, other_point);

        // why build a type table if it isn't going to do anything?
        if (point_info.argument_types != other_point_info.argument_types)
        {
            BLT_INFO("---------\\{Begin Point {} to {}}---------", point.get_spoint(), other_point.get_spoint());
            BLT_DEBUG("Our argument types (Terminal? {}; Ephemeral? {}):", point_info.argc.argc == 0,
                      tree->operations[point.get_spoint()].get_flags().is_ephemeral());
            for (const auto [i, type] : enumerate(point_info.argument_types))
                BLT_TRACE("\t{} -> {} (Aka '{}')", i, type, tree->m_program->get_typesystem().get_type(type).name());
            BLT_DEBUG("Other argument types (Terminal? {}; Ephemeral? {}):", other_point_info.argc.argc == 0,
                      other_tree.operations[other_point.get_spoint()].get_flags().is_ephemeral());
            for (const auto [i, type] : enumerate(other_point_info.argument_types))
                BLT_TRACE("\t{} -> {} (Aka '{}')", i, type, tree->m_program->get_typesystem().get_type(type).name());
            BLT_TRACE("");

            // const auto our_end_point_iter = tree->operations.begin() + extent;
            // const auto other_end_point_iter = other_tree.operations.begin() + other_extent;

            // const auto our_bytes_after = calculate_ephemeral_size(our_end_point_iter, tree->operations.end());
            // const auto other_bytes_after = calculate_ephemeral_size(other_end_point_iter, other_tree.operations.end());

            thread_local struct storage_t
            {
                tree_t temp;

                buffer_wrapper_t our_after{};
                buffer_wrapper_t other_after{};
                // argument index -> type_id for missing types
                std::vector<std::tuple<size_t, type_id, bool_flag_t>> our_types;
                std::vector<std::tuple<size_t, type_id, bool_flag_t>> other_types;

                std::vector<std::pair<size_t, size_t>> our_swaps;
                std::vector<std::pair<size_t, size_t>> other_swaps;

                std::vector<child_t> our_children;
                std::vector<child_t> other_children;

                std::vector<type_id> our_current_types;
                std::vector<type_id> other_current_types;

                explicit storage_t(tree_t tree): temp(std::move(tree))
                {
                }
            } storage{tree_t{*tree->m_program}};
            storage.temp.clear(*tree->m_program);
            storage.our_types.clear();
            storage.other_types.clear();
            storage.our_swaps.clear();
            storage.other_swaps.clear();
            storage.our_children.clear();
            storage.other_children.clear();
            storage.our_current_types.clear();
            storage.other_current_types.clear();
            storage.our_current_types.reserve(point_info.argument_types.size());
            storage.other_current_types.reserve(other_point_info.argument_types.size());

            // const auto our_after_ptr = storage.our_after.get(our_bytes_after);
            // const auto other_after_ptr = storage.other_after.get(other_bytes_after);

            // save a copy of all the data after the operator subtrees
            // tree->values.copy_to(our_after_ptr, our_bytes_after);
            // tree->values.pop_bytes(our_bytes_after);
            // other_tree.values.copy_to(other_after_ptr, other_bytes_after);
            // other_tree.values.pop_bytes(other_bytes_after);

            const auto common_size = std::min(point_info.argument_types.size(), other_point_info.argument_types.size());
            for (size_t i = 0; i < common_size; ++i)
            {
                if (point_info.argument_types[i] != other_point_info.argument_types[i])
                {
                    storage.our_types.emplace_back(i, point_info.argument_types[i], false);
                    storage.other_types.emplace_back(i, other_point_info.argument_types[i], false);
                }
                storage.our_current_types.emplace_back(point_info.argument_types[i]);
                storage.other_current_types.emplace_back(other_point_info.argument_types[i]);
            }
            for (size_t i = common_size; i < point_info.argument_types.size(); ++i)
            {
                storage.our_types.emplace_back(i, point_info.argument_types[i], false);
                storage.our_current_types.emplace_back(point_info.argument_types[i]);
            }
            for (size_t i = common_size; i < other_point_info.argument_types.size(); ++i)
            {
                storage.other_types.emplace_back(i, other_point_info.argument_types[i], false);
                storage.other_current_types.emplace_back(other_point_info.argument_types[i]);
            }

            for (const auto& [index, type, _] : storage.our_types)
            {
                for (auto& [other_index, other_type, consumed] : storage.other_types)
                {
                    if (other_type == type && !consumed)
                    {
                        storage.other_swaps.emplace_back(index, other_index);
                        BLT_TRACE("[Others] Swaps found! {} to {}", index, other_index);
                        consumed = true;
                        break;
                    }
                }
            }
            for (const auto& [index, type, _] : storage.other_types)
            {
                for (auto& [other_index, other_type, consumed] : storage.our_types)
                {
                    if (other_type == type && !consumed)
                    {
                        BLT_TRACE("[Ours] Swaps found! {} to {}", index, other_index);
                        storage.our_swaps.emplace_back(index, other_index);
                        consumed = true;
                        break;
                    }
                }
            }

            BLT_TRACE("");

            tree->find_child_extends(storage.our_children, point.get_point(), point_info.argc.argc);

            // apply the swaps
            for (const auto& [i, j] : storage.our_swaps)
            {
                swap_subtrees(storage.our_children[i], storage.our_children[j]);
                const auto s1 = storage.our_children[i].size();
                const auto s2 = storage.our_children[j].size();
                storage.our_children[i].end = storage.our_children[i].start + s1;
                storage.our_children[j].end = storage.our_children[j].start + s2;

                const auto tmp = storage.our_current_types[i];
                storage.our_current_types[i] = storage.our_current_types[j];
                storage.our_current_types[j] = tmp;
            }

            other_tree.find_child_extends(storage.other_children, other_point.get_point(), other_point_info.argc.argc);

            for (const auto& [i, j] : storage.other_swaps)
            {
                swap_subtrees(storage.other_children[i], storage.other_children[j]);
                const auto s1 = storage.other_children[i].size();
                const auto s2 = storage.other_children[j].size();
                storage.other_children[i].end = storage.other_children[i].start + s1;
                storage.other_children[j].end = storage.other_children[j].start + s2;

                const auto tmp = storage.other_current_types[i];
                storage.other_current_types[i] = storage.other_current_types[j];
                storage.other_current_types[j] = tmp;
            }

            for (size_t i = 0; i < common_size; i++)
            {
                if (storage.our_current_types[i] != other_point_info.argument_types[i])
                {
                    BLT_TRACE("[Our] Mismatched type at index {} found current type '{}' expected type '{}'", i,
                              tree->m_program->get_typesystem().get_type(storage.our_current_types[i]).name(),
                              tree->m_program->get_typesystem().get_type(other_point_info.argument_types[i]).name());
                    for (auto& [index, type, consumed] : storage.other_types)
                    {
                        if (consumed)
                            continue;
                        if (type == other_point_info.argument_types[i])
                        {
                            BLT_TRACE("Consuming other type {} at index {} ", type, index);
                            consumed = true;
                            const auto s1 = storage.other_children[index].size();
                            const auto s2 = storage.our_children[i].size();
                            swap_subtrees(storage.our_children[i], other_tree, storage.other_children[index]);
                            storage.our_children[i].end = storage.our_children[i].start + s1;
                            storage.other_children[index].end = storage.other_children[index].start + s2;
                            auto old_type = storage.our_current_types[i];
                            storage.our_current_types[i] = type;
                            storage.other_current_types[index] = old_type;
                            goto b1;
                        }
                    }
#if BLT_DEBUG_LEVEL >= 1
                    BLT_ERROR("Unable to find type for position {}, expected type '{}' but found type '{}'. ", i, other_point_info.argument_types[i],
                              storage.our_current_types[i]);
                    BLT_ABORT("Failure state in swap operators!");
#endif
                b1:
                    {
                    }
                }
                if (storage.other_current_types[i] != point_info.argument_types[i])
                {
                    BLT_TRACE("[Other] Mismatched type at index {} found current type '{}' expected type '{}'", i,
                              tree->m_program->get_typesystem().get_type(storage.other_current_types[i]).name(),
                              tree->m_program->get_typesystem().get_type(point_info.argument_types[i]).name());
                    for (auto& [index, type, consumed] : storage.our_types)
                    {
                        if (consumed)
                            continue;
                        if (type == point_info.argument_types[i])
                        {
                            BLT_TRACE("Consuming our type {} at index {} ", type, index);
                            consumed = true;
                            const auto s1 = storage.our_children[index].size();
                            const auto s2 = storage.other_children[i].size();
                            other_tree.manipulate().easy_manipulator().swap_subtrees(storage.other_children[i], *tree, storage.our_children[index]);
                            storage.other_children[i].end = storage.other_children[i].start + s1;
                            storage.our_children[index].end = storage.our_children[index].start + s2;
                            auto old_type = storage.other_current_types[i];
                            storage.other_current_types[i] = type;
                            storage.our_current_types[index] = old_type;
                            goto b2;
                        }
                    }
#if BLT_DEBUG_LEVEL >= 1
                    BLT_WARN("Unable to find type for position {}, expected type {} but found type {}. ", i, point_info.argument_types[i],
                             storage.other_current_types[i]);
                    BLT_ABORT("Failure state in swap operators!");
#endif
                b2:
                    {
                    }
                }
            }

#if BLT_DEBUG_LEVEL >= 1
            for (const auto& [i, a, b] : in_pairs(storage.our_current_types, other_point_info.argument_types).enumerate().take(common_size).flatten())
                BLT_ASSERT_MSG(a == b, ("[Our] Mismatched types at index " + std::to_string(i) + " expected '" + std::string(tree->m_program->get_typesystem().get_type(a).name()) + "' but found '" +
                               std::string(tree->m_program->get_typesystem().get_type(b).name()) + "'").c_str());
            for (const auto& [i, a, b] : in_pairs(storage.other_current_types, point_info.argument_types).enumerate().take(common_size).flatten())
                BLT_ASSERT_MSG(a == b, ("[Other] Mismatched types at index " + std::to_string(i) + " expected '" + std::string(tree->m_program->get_typesystem().get_type(a).name()) + "' but found '" +
                               std::string(tree->m_program->get_typesystem().get_type(b).name()) + "'").c_str());
#endif

            auto insert_index = storage.other_children.back().end;
            for (size_t i = common_size; i < point_info.argument_types.size(); i++)
            {
                for (auto& [index, type, consumed] : storage.our_types)
                {
                    if (consumed)
                        continue;
                    if (index != i)
                        continue;
                    if (type == point_info.argument_types[i])
                    {
                        // BLT_TRACE("[Our] Consuming type {} at index {} being inserted into {}", type, index, insert_index);
                        storage.temp.clear(*tree->m_program);
                        copy_subtree(storage.our_children[i], storage.temp);
                        insert_index = other_tree.manipulate().easy_manipulator().insert_subtree(subtree_point_t{insert_index}, storage.temp);
                        consumed = bool_flag_t::TRUE_BUT_REMOVE;
                        goto b3;
                    }
                }
#if BLT_DEBUG_LEVEL >= 1
                BLT_WARN("Unable to find type for position {}, expected type {}", i, point_info.argument_types[i]);
                BLT_ABORT("Failure state in swap operators!");
#endif
            b3:
                {
                }
            }

            insert_index = storage.our_children.back().end;
            for (size_t i = common_size; i < other_point_info.argument_types.size(); i++)
            {
                for (auto& [index, type, consumed] : storage.other_types)
                {
                    if (consumed)
                        continue;
                    if (index != i)
                        continue;
                    if (type == other_point_info.argument_types[i])
                    {
                        // BLT_TRACE("[Other] Consuming type {} at index {} being inserted into {}", type, index, insert_index);
                        storage.temp.clear(*tree->m_program);
                        other_tree.manipulate().easy_manipulator().copy_subtree(storage.other_children[i], storage.temp);
                        insert_index = insert_subtree(subtree_point_t{insert_index}, storage.temp);
                        consumed = bool_flag_t::TRUE_BUT_REMOVE;
                        goto b4;
                    }
                }
            b4:
                {
                }
            }

            for (auto& [index, type, consumed] : iterate(storage.our_types).rev())
            {
                if (consumed.should_remove())
                    delete_subtree(storage.our_children[index]);
            }

            for (auto& [index, type, consumed] : iterate(storage.other_types).rev())
            {
                if (consumed.should_remove())
                    other_tree.manipulate().easy_manipulator().delete_subtree(storage.other_children[index]);
            }
        }
        auto op = tree->operations[point.get_spoint()];
        tree->operations[point.get_spoint()] = other_tree.operations[other_point.get_spoint()];
        other_tree.operations[other_point.get_spoint()] = op;

#if BLT_DEBUG_LEVEL >= 2
        if (!tree->check(detail::debug::context_ptr) || !other_tree.check(detail::debug::context_ptr))
            throw std::runtime_error("Tree check failed");
#endif
    }

    void slow_tree_manipulator_t::swap_operators(const size_t point, tree_t& other_tree, const size_t other_point) const
    {
        swap_operators(subtree_point_t{point}, other_tree, subtree_point_t{other_point});
    }


    ptrdiff_t tree_t::find_endpoint(ptrdiff_t start) const
    {
        i64 children_left = 0;

        do
        {
            const auto& type = m_program->get_operator_info(operations[start].id());
            // this is a child to someone
            if (children_left != 0)
                children_left--;
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            start++;
        }
        while (children_left > 0);

        return start;
    }

    tree_t& tree_t::get_thread_local(gp_program& program)
    {
        thread_local tree_t tree{program};
        tree.clear(program);
        return tree;
    }

    void tree_t::handle_operator_inserted(const op_container_t& op)
    {
        if (m_program->is_operator_ephemeral(op.id()))
        {
            // Ephemeral values have corresponding insertions into the stack
            m_program->get_operator_info(op.id()).func(nullptr, values, values);
            if (m_program->operator_has_ephemeral_drop(op.id()))
            {
                auto [_, ptr] = values.access_pointer(op.type_size(), op.type_size());
                ptr = new std::atomic_uint64_t(1);
                ptr.bit(0, true);
            }
        }
    }

    void tree_t::handle_ptr_empty(const mem::pointer_storage<std::atomic_uint64_t>& ptr, u8* data, const operator_id id) const
    {
        m_program->get_destroy_func(id)(detail::destroy_t::RETURN, data);
        delete ptr.get();
        // BLT_INFO("Deleting pointer!");
    }

    evaluation_context& tree_t::evaluate(void* ptr) const
    {
        return m_program->get_eval_func()(*this, ptr);
    }

    bool tree_t::check(void* context) const
    {
        size_t bytes_expected = 0;
        const auto bytes_size = values.stored();

        for (const auto& op : operations)
        {
            if (op.is_value())
                bytes_expected += op.type_size();
        }

        if (bytes_expected != bytes_size)
        {
            BLT_ERROR("Stack state: Stored: {}; Capacity: {}; Remainder: {}", values.stored(), values.capacity(), values.remainder());
            BLT_ERROR("Child tree bytes {} vs expected {}, difference: {}", bytes_size, bytes_expected,
                      static_cast<ptrdiff_t>(bytes_expected) - static_cast<ptrdiff_t>(bytes_size));
            BLT_ERROR("Amount of bytes in stack doesn't match the number of bytes expected for the operations");
            return false;
        }

        size_t total_produced = 0;
        size_t total_consumed = 0;
        size_t index = 0;

        try
        {
            // copy the initial values
            evaluation_context results{};

            auto value_stack = values;
            auto& values_process = results.values;

            for (const auto& operation : iterate(operations).rev())
            {
                ++index;
                if (operation.is_value())
                {
                    value_stack.transfer_bytes(values_process, operation.type_size());
                    total_produced += operation.type_size();
                    continue;
                }
                auto& info = m_program->get_operator_info(operation.id());
                for (auto& arg : info.argument_types)
                    total_consumed += m_program->get_typesystem().get_type(arg).size();
                m_program->get_operator_info(operation.id()).func(context, values_process, values_process);
                total_produced += m_program->get_typesystem().get_type(info.return_type).size();
            }

            const auto v1 = static_cast<ptrdiff_t>(results.values.stored());
            const auto v2 = static_cast<ptrdiff_t>(operations.front().type_size());

            // ephemeral don't need to be dropped as there are no copies which matter when checking the tree
            if (!operations.front().get_flags().is_ephemeral())
                m_program->get_destroy_func(operations.front().id())(detail::destroy_t::RETURN, results.values.from(operations.front().type_size()));
            if (v1 != v2)
            {
                const auto vd = std::abs(v1 - v2);
                BLT_ERROR("Tree result stack has {} bytes expected {} bytes, total difference: {}", v1, v2, vd);
                BLT_ERROR("Total Produced {} || Total Consumed {} || Total Difference {}", total_produced, total_consumed,
                          std::abs(static_cast<blt::ptrdiff_t>(total_produced) - static_cast<blt::ptrdiff_t>(total_consumed)));
                return false;
            }
        }
        catch (std::exception& e)
        {
            BLT_ERROR("Exception occurred \"{}\"", e.what());
            BLT_ERROR("Total Produced {} || Total Consumed {} || Total Difference {}", total_produced, total_consumed,
                      std::abs(static_cast<blt::ptrdiff_t>(total_produced) - static_cast<blt::ptrdiff_t>(total_consumed)));
            BLT_ERROR("We failed at index {}", index);
            return false;
        }
        return true;
    }

    void tree_t::find_child_extends(tracked_vector<child_t>& vec, const size_t parent_node, const size_t argc) const
    {
        BLT_ASSERT_MSG(vec.empty(), "Vector to find_child_extends should be empty!");
        while (vec.size() < argc)
        {
            const auto current_point = vec.size();
            child_t prev; // NOLINT
            if (current_point == 0)
            {
                // first child.
                prev = {
                    static_cast<ptrdiff_t>(parent_node + 1),
                    find_endpoint(static_cast<ptrdiff_t>(parent_node + 1))
                };
                vec.push_back(prev);
                continue;
            }
            prev = vec[current_point - 1];
            child_t next = {prev.end, find_endpoint(prev.end)};
            vec.push_back(next);
        }
    }

    temporary_tree_storage_t::temporary_tree_storage_t(tree_t& tree): operations(&tree.operations), values(&tree.values)
    {
    }

    type_id subtree_point_t::get_type() const
    {
#if BLT_DEBUG_LEVEL > 0
        if (info == nullptr)
            throw std::runtime_error(
                "Invalid subtree point, operator info was null! (Point probably created with passthrough "
                "intentions or operator info was not available, please manually acquire type)");
#endif
        return info->return_type;
    }

    void single_operation_tree_manipulator_t::replace_subtree(tree_t& other_tree)
    {
        replace_subtree(other_tree.operations, other_tree.values);
    }

    void single_operation_tree_manipulator_t::replace_subtree(tracked_vector<op_container_t>& operations, stack_allocator& stack)
    {
    }

    void tree_t::copy_fast(const tree_t& copy)
    {
        if (this == &copy)
            return;

        operations.reserve(copy.operations.size());

        auto copy_it = copy.operations.begin();
        auto op_it = operations.begin();

        size_t total_op_bytes = 0;
        size_t total_copy_bytes = 0;

        for (; op_it != operations.end(); ++op_it)
        {
            if (copy_it == copy.operations.end())
                break;
            if (copy_it->is_value())
            {
                copy.handle_refcount_increment(copy_it, total_copy_bytes);
                total_copy_bytes += copy_it->type_size();
            }
            if (op_it->is_value())
            {
                handle_refcount_decrement(op_it, total_op_bytes);
                total_op_bytes += op_it->type_size();
            }
            *op_it = *copy_it;
            ++copy_it;
        }
        const auto op_it_cpy = op_it;
        for (; op_it != operations.end(); ++op_it)
        {
            if (op_it->is_value())
            {
                handle_refcount_decrement(op_it, total_op_bytes);
                total_op_bytes += op_it->type_size();
            }
        }
        operations.erase(op_it_cpy, operations.end());
        for (; copy_it != copy.operations.end(); ++copy_it)
        {
            if (copy_it->is_value())
            {
                copy.handle_refcount_increment(copy_it, total_copy_bytes);
                total_copy_bytes += copy_it->type_size();
            }
            operations.emplace_back(*copy_it);
        }

        values.reserve(copy.values.stored());
        values.reset();
        values.insert(copy.values);
    }

    void tree_t::clear(gp_program& program)
    {
        auto* f = &program;
        if (&program != m_program)
            m_program = f;
        size_t total_bytes = 0;
        for (const auto& op : iterate(operations))
        {
            if (op.is_value())
            {
                if (op.get_flags().is_ephemeral() && op.has_ephemeral_drop())
                {
                    auto [val, ptr] = values.access_pointer_forward(total_bytes, op.type_size());
                    --*ptr;
                    if (*ptr == 0)
                        handle_ptr_empty(ptr, val, op.id());
                }
                total_bytes += op.type_size();
            }
        }
        operations.clear();
        values.reset();
    }

    void tree_t::insert_operator(const size_t index, const op_container_t& container)
    {
        if (container.get_flags().is_ephemeral())
        {
            byte_only_transaction_t move{*this, total_value_bytes(index)};
            handle_operator_inserted(container);
        }
        operations.insert(operations.begin() + static_cast<ptrdiff_t>(index), container);
    }

    subtree_point_t tree_t::subtree_from_point(const ptrdiff_t point) const
    {
        return subtree_point_t{point, m_program->get_operator_info(operations[point].id())};
    }

    size_t tree_t::required_size() const
    {
        // 2 size_t used to store expected_length of operations + size of the values stack
        return 2 * sizeof(size_t) + operations.size() * sizeof(size_t) + values.stored();
    }

    void tree_t::to_byte_array(std::byte* out) const
    {
        const auto op_size = operations.size();
        std::memcpy(out, &op_size, sizeof(size_t));
        out += sizeof(size_t);
        for (const auto& op : operations)
        {
            constexpr auto size_of_op = sizeof(operator_id);
            auto id = op.id();
            std::memcpy(out, &id, size_of_op);
            out += size_of_op;
        }
        const auto val_size = values.stored();
        std::memcpy(out, &val_size, sizeof(size_t));
        out += sizeof(size_t);
        std::memcpy(out, values.data(), val_size);
    }

    void tree_t::to_file(fs::writer_t& file) const
    {
        const auto op_size = operations.size();
        BLT_ASSERT(file.write(&op_size, sizeof(size_t)) == sizeof(size_t));
        for (const auto& op : operations)
        {
            auto id = op.id();
            file.write(&id, sizeof(operator_id));
        }
        const auto val_size = values.stored();
        BLT_ASSERT(file.write(&val_size, sizeof(size_t)) == sizeof(size_t));
        BLT_ASSERT(file.write(values.data(), val_size) == static_cast<i64>(val_size));
    }

    void tree_t::from_byte_array(const std::byte* in)
    {
        size_t ops_to_read;
        std::memcpy(&ops_to_read, in, sizeof(size_t));
        in += sizeof(size_t);
        operations.reserve(ops_to_read);
        for (size_t i = 0; i < ops_to_read; i++)
        {
            operator_id id;
            std::memcpy(&id, in, sizeof(operator_id));
            in += sizeof(operator_id);
            operations.emplace_back(
                m_program->get_typesystem().get_type(m_program->get_operator_info(id).return_type).size(),
                id,
                m_program->is_operator_ephemeral(id),
                m_program->get_operator_flags(id)
            );
        }
        size_t val_size;
        std::memcpy(&val_size, in, sizeof(size_t));
        in += sizeof(size_t);
        // TODO replace instances of u8 that are used to alias types with the proper std::byte
        values.copy_from(reinterpret_cast<const u8*>(in), val_size);
    }

    void tree_t::from_file(fs::reader_t& file)
    {
        size_t ops_to_read;
        BLT_ASSERT(file.read(&ops_to_read, sizeof(size_t)) == sizeof(size_t));
        operations.reserve(ops_to_read);
        for (size_t i = 0; i < ops_to_read; i++)
        {
            operator_id id;
            BLT_ASSERT(file.read(&id, sizeof(operator_id)) == sizeof(operator_id));
            operations.emplace_back(
                m_program->get_typesystem().get_type(m_program->get_operator_info(id).return_type).size(),
                id,
                m_program->is_operator_ephemeral(id),
                m_program->get_operator_flags(id)
            );
        }
        size_t bytes_in_head;
        BLT_ASSERT(file.read(&bytes_in_head, sizeof(size_t)) == sizeof(size_t));
        values.resize(bytes_in_head);
        BLT_ASSERT(file.read(values.data(), bytes_in_head) == static_cast<i64>(bytes_in_head));
    }

    bool operator==(const tree_t& a, const tree_t& b)
    {
        if (a.operations.size() != b.operations.size())
            return false;
        if (a.values.stored() != b.values.stored())
            return false;
        return std::equal(a.operations.begin(), a.operations.end(), b.operations.begin());
    }

    bool operator==(const op_container_t& a, const op_container_t& b)
    {
        return a.id() == b.id();
    }

    bool operator==(const individual_t& a, const individual_t& b)
    {
        return a.tree == b.tree;
    }
}
