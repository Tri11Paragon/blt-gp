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
#include <blt/std/logging.h>
#include <blt/gp/program.h>
#include <stack>

namespace blt::gp
{
    template <typename>
    static u8* get_thread_pointer_for_size(const size_t bytes)
    {
        thread_local expanding_buffer<u8> buffer;
        if (bytes > buffer.size())
            buffer.resize(bytes);
        return buffer.data();
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

    void tree_t::print(std::ostream& out, bool print_literals, bool pretty_print, bool include_types) const
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
        for (const auto& v : operations)
        {
            auto info = m_program->get_operator_info(v.id());
            const auto name = m_program->get_name(v.id()) ? m_program->get_name(v.id()).value() : "NULL";
            auto return_type = get_return_type(*m_program, info.return_type, include_types);
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
                auto top = arguments_left.top();
                arguments_left.pop();
                if (top == 0)
                {
                    indent--;
                    create_indent(out, indent, pretty_print) << ")" << end_indent(pretty_print);
                    continue;
                }
                else
                {
                    if (!pretty_print)
                        out << " ";
                    arguments_left.push(top - 1);
                    break;
                }
            }
        }
        while (!arguments_left.empty())
        {
            auto top = arguments_left.top();
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

    tree_t::subtree_point_t tree_t::select_subtree(const double terminal_chance) const
    {
        do
        {
            const auto point = m_program->get_random().get_u64(0, operations.size());
            const auto& info = m_program->get_operator_info(operations[point].id());
            if (!info.argc.is_terminal())
                return {static_cast<ptrdiff_t>(point), info.return_type};
            if (m_program->get_random().choice(terminal_chance))
                return {static_cast<ptrdiff_t>(point), info.return_type};
        }
        while (true);
    }

    std::optional<tree_t::subtree_point_t> tree_t::select_subtree(const type_id type, const u32 max_tries, const double terminal_chance) const
    {
        for (u32 i = 0; i < max_tries; ++i)
        {
            if (const auto tree = select_subtree(terminal_chance); tree.type == type)
                return tree;
        }
        return {};
    }

    tree_t::subtree_point_t tree_t::select_subtree_traverse(const double terminal_chance, const double depth_multiplier) const
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
                    return {static_cast<ptrdiff_t>(index), info.return_type};
                index = 0;
                depth = 0;
                exit_chance = 0;
                continue;
            }
            if (m_program->get_random().choice(exit_chance))
                return {static_cast<ptrdiff_t>(index), info.return_type};

            const auto child = m_program->get_random().get_u32(0, info.argc.argc);
            index++;
            for (u32 i = 0; i < child; i++)
                index = find_endpoint(static_cast<ptrdiff_t>(index));

            ++depth;
            exit_chance = 1.0 - (1.0 / (1 + depth * depth_multiplier * 0.5));
        }
    }

    std::optional<tree_t::subtree_point_t> tree_t::select_subtree_traverse(const type_id type, const u32 max_tries, const double terminal_chance,
                                                                           const double depth_multiplier) const
    {
        for (u32 i = 0; i < max_tries; ++i)
        {
            if (const auto tree = select_subtree_traverse(terminal_chance, depth_multiplier); tree.type == type)
                return tree;
        }
        return {};
    }

    void tree_t::copy_subtree(const subtree_point_t point, std::vector<op_container_t>& operators, stack_allocator& stack)
    {
        const auto point_begin_itr = operations.begin() + point.pos;
        const auto point_end_itr = operations.begin() + find_endpoint(point.pos);

        const size_t after_bytes = accumulate_type_sizes(point_end_itr, operations.end());

        operators.reserve(operators.size() + std::distance(point_begin_itr, point_end_itr));
        size_t for_bytes = 0;
        for (auto& it : iterate(point_begin_itr, point_end_itr).rev())
        {
            if (it.is_value())
            {
                for_bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto& ptr = values.access_pointer(for_bytes + after_bytes, it.type_size());
                    ++*ptr;
                }
            }
            operators.emplace_back(it);
        }

        stack.copy_from(values, for_bytes, after_bytes);
    }

    void tree_t::swap_subtrees(const subtree_point_t our_subtree, tree_t& other_tree, const subtree_point_t other_subtree)
    {
        const auto our_point_begin_itr = operations.begin() + our_subtree.pos;
        const auto our_point_end_itr = operations.begin() + find_endpoint(our_subtree.pos);

        const auto other_point_begin_itr = other_tree.operations.begin() + other_subtree.pos;
        const auto other_point_end_itr = other_tree.operations.begin() + other_tree.find_endpoint(other_subtree.pos);

        thread_local tracked_vector<op_container_t> c1_operators;
        thread_local tracked_vector<op_container_t> c2_operators;
        c1_operators.clear();
        c2_operators.clear();

        c1_operators.reserve(std::distance(our_point_begin_itr, our_point_end_itr));
        c2_operators.reserve(std::distance(other_point_begin_itr, other_point_end_itr));

        // i don't think this is required for swapping values, since the total number of additions is net zero
        // the tree isn't destroyed at any point.

        size_t for_our_bytes = 0;
        for (const auto& it : iterate(our_point_begin_itr, our_point_end_itr))
        {
            if (it.is_value())
            {
                // if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                // {
                //     auto& ptr = values.access_pointer_forward(for_our_bytes, it.type_size());
                //     ++*ptr;
                // }
                for_our_bytes += it.type_size();
            }
            c1_operators.emplace_back(it);
        }

        size_t for_other_bytes = 0;
        for (const auto& it : iterate(other_point_begin_itr, other_point_end_itr))
        {
            if (it.is_value())
            {
                // if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                // {
                //     auto& ptr = values.access_pointer_forward(for_other_bytes, it.type_size());
                //     ++*ptr;
                // }
                for_other_bytes += it.type_size();
            }
            c2_operators.emplace_back(it);
        }

        const size_t c1_stack_after_bytes = accumulate_type_sizes(our_point_end_itr, operations.end());
        const size_t c2_stack_after_bytes = accumulate_type_sizes(other_point_end_itr, other_tree.operations.end());
        const auto c1_total = static_cast<ptrdiff_t>(c1_stack_after_bytes + for_our_bytes);
        const auto c2_total = static_cast<ptrdiff_t>(c2_stack_after_bytes + for_other_bytes);
        const auto copy_ptr_c1 = get_thread_pointer_for_size<struct c1_t>(c1_total);
        const auto copy_ptr_c2 = get_thread_pointer_for_size<struct c2_t>(c2_total);

        values.reserve(values.bytes_in_head() - for_our_bytes + for_other_bytes);
        other_tree.values.reserve(other_tree.values.bytes_in_head() - for_other_bytes + for_our_bytes);

        values.copy_to(copy_ptr_c1, c1_total);
        values.pop_bytes(c1_total);

        other_tree.values.copy_to(copy_ptr_c2, c2_total);
        other_tree.values.pop_bytes(c2_total);

        other_tree.values.copy_from(copy_ptr_c1, for_our_bytes);
        other_tree.values.copy_from(copy_ptr_c2 + for_other_bytes, c2_stack_after_bytes);

        values.copy_from(copy_ptr_c2, for_other_bytes);
        values.copy_from(copy_ptr_c1 + for_our_bytes, c1_stack_after_bytes);

        // now swap the operators
        auto insert_point_c1 = our_point_begin_itr - 1;
        auto insert_point_c2 = other_point_begin_itr - 1;

        // invalidates [begin, end()) so the insert points should be fine
        operations.erase(our_point_begin_itr, our_point_end_itr);
        other_tree.operations.erase(other_point_begin_itr, other_point_end_itr);

        operations.insert(++insert_point_c1, c2_operators.begin(), c2_operators.end());
        other_tree.operations.insert(++insert_point_c2, c1_operators.begin(), c1_operators.end());
    }

    void tree_t::replace_subtree(const subtree_point_t point, const ptrdiff_t extent, tree_t& other_tree)
    {
        const auto point_begin_itr = operations.begin() + point.pos;
        const auto point_end_itr = operations.begin() + extent;

        const size_t after_bytes = accumulate_type_sizes(point_end_itr, operations.end());

        size_t for_bytes = 0;
        for (auto& it : iterate(point_begin_itr, point_end_itr).rev())
        {
            if (it.is_value())
            {
                for_bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto& ptr = values.access_pointer(for_bytes + after_bytes, it.type_size());
                    --*ptr;
                    if (*ptr == 0)
                    {
                        // TODO
                    }
                }
            }
        }
        auto insert = operations.erase(point_begin_itr, point_end_itr);

        const auto ptr = get_thread_pointer_for_size<struct replace>(after_bytes);
        values.copy_to(ptr, after_bytes);
        values.pop_bytes(after_bytes + for_bytes);

        size_t copy_bytes = 0;
        for (const auto& v : other_tree.operations)
        {
            if (v.is_value())
            {
                if (v.get_flags().is_ephemeral() && v.has_ephemeral_drop())
                {
                    auto& pointer = other_tree.values.access_pointer(copy_bytes, v.type_size());
                    --*pointer;
                }
                copy_bytes += v.type_size();
            }
            insert = ++operations.emplace(insert, v);
        }

        values.insert(other_tree.values);
        values.copy_from(ptr, after_bytes);
    }

    void tree_t::delete_subtree(const subtree_point_t point, const ptrdiff_t extent)
    {
        const auto point_begin_itr = operations.begin() + point.pos;
        const auto point_end_itr = operations.begin() + extent;

        const size_t after_bytes = accumulate_type_sizes(point_end_itr, operations.end());

        size_t for_bytes = 0;
        for (auto& it : iterate(point_begin_itr, point_end_itr).rev())
        {
            if (it.is_value())
            {
                for_bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto& ptr = values.access_pointer(for_bytes + after_bytes, it.type_size());
                    --*ptr;
                    if (*ptr == 0)
                    {
                        // TODO
                    }
                }
            }
        }
        operations.erase(point_begin_itr, point_end_itr);

        const auto ptr = get_thread_pointer_for_size<struct replace>(after_bytes);
        values.copy_to(ptr, after_bytes);
        values.pop_bytes(after_bytes + for_bytes);
        values.copy_from(ptr, after_bytes);
    }

    ptrdiff_t tree_t::insert_subtree(const subtree_point_t point, tree_t& other_tree)
    {
        const size_t after_bytes = accumulate_type_sizes(operations.begin() + point.pos, operations.end());
        auto move = temporary_move(after_bytes);

        auto insert = operations.begin() + point.pos;
        size_t bytes = 0;
        for (auto& it : iterate(other_tree.operations).rev())
        {
            if (it.is_value())
            {
                bytes += it.type_size();
                if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                {
                    auto& ptr = other_tree.values.access_pointer(bytes, it.type_size());
                    ++*ptr;
                }
            }
            insert = operations.insert(insert, it);
        }
        values.insert(other_tree.values);

        return static_cast<ptrdiff_t>(point.pos + other_tree.size());
    }

    tree_t::after_bytes_data_t tree_t::temporary_move(const size_t bytes)
    {
        const auto data = get_thread_pointer_for_size<struct temporary_move>(bytes);
        values.copy_to(data, bytes);
        values.pop_bytes(bytes);
        return after_bytes_data_t{*this, data, bytes};
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

    void tree_t::handle_operator_inserted(const op_container_t& op)
    {
        if (m_program->is_operator_ephemeral(op.id()))
        {
            // Ephemeral values have corresponding insertions into the stack
            m_program->get_operator_info(op.id()).func(nullptr, values, values);
            if (m_program->operator_has_ephemeral_drop(op.id()))
            {
                auto& ptr = values.access_pointer(op.type_size(), op.type_size());
                ptr = new std::atomic_uint64_t(1);
                ptr.bit(0, true);
            }
        }
    }

    evaluation_context& tree_t::evaluate(void* ptr) const
    {
        return m_program->get_eval_func()(*this, ptr);
    }

    bool tree_t::check(void* context) const
    {
        blt::size_t bytes_expected = 0;
        const auto bytes_size = values.size().total_used_bytes;

        for (const auto& op : operations)
        {
            if (op.is_value())
                bytes_expected += op.type_size();
        }

        if (bytes_expected != bytes_size)
        {
            BLT_WARN_STREAM << "Stack state: " << values.size() << "\n";
            BLT_WARN("Child tree bytes %ld vs expected %ld, difference: %ld", bytes_size, bytes_expected,
                     static_cast<blt::ptrdiff_t>(bytes_expected) - static_cast<blt::ptrdiff_t>(bytes_size));
            BLT_WARN("Amount of bytes in stack doesn't match the number of bytes expected for the operations");
            return false;
        }

        // copy the initial values
        evaluation_context results{};

        auto value_stack = values;
        auto& values_process = results.values;

        size_t total_produced = 0;
        size_t total_consumed = 0;

        for (const auto& operation : iterate(operations).rev())
        {
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

        const auto v1 = results.values.bytes_in_head();
        const auto v2 = static_cast<ptrdiff_t>(operations.front().type_size());

        m_program->get_destroy_func(operations.front().id())(detail::destroy_t::RETURN, results.values);
        if (v1 != v2)
        {
            const auto vd = std::abs(v1 - v2);
            BLT_ERROR("found %ld bytes expected %ld bytes, total difference: %ld", v1, v2, vd);
            BLT_ERROR("Total Produced %ld || Total Consumed %ld || Total Difference %ld", total_produced, total_consumed,
                      std::abs(static_cast<blt::ptrdiff_t>(total_produced) - static_cast<blt::ptrdiff_t>(total_consumed)));
            return false;
        }
        return true;
    }

    void tree_t::find_child_extends(tracked_vector<child_t>& vec, const size_t parent_node, const size_t argc) const
    {
        while (vec.size() < argc)
        {
            const auto current_point = vec.size();
            child_t prev{};
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
                    auto& ptr = values.access_pointer_forward(total_bytes, op.type_size());
                    --*ptr;
                    // TODO
                    // BLT_TRACE(ptr->load());
                    // if (*ptr == 0)
                    // {
                        // BLT_TRACE("Deleting pointers!");
                        // delete ptr.get();
                    // }
                }
                total_bytes += op.type_size();
            }
        }
        operations.clear();
        values.reset();
    }

    tree_t::subtree_point_t tree_t::subtree_from_point(ptrdiff_t point) const
    {
        return {point, m_program->get_operator_info(operations[point].id()).return_type};
    }

    void tree_t::modify_operator(const size_t point, operator_id new_id, std::optional<type_id> return_type)
    {
        if (!return_type)
            return_type = m_program->get_operator_info(new_id).return_type;
        operations[point] = {
            m_program->get_typesystem().get_type(*return_type).size(),
            new_id,
            m_program->is_operator_ephemeral(new_id),
            m_program->get_operator_flags(new_id)
        };
    }
}
