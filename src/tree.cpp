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

    void tree_t::byte_only_transaction_t::move(const size_t bytes_to_move)
    {
        bytes = bytes_to_move;
        data = get_thread_pointer_for_size<struct move_tempoary_bytes>(bytes);
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

    void tree_t::copy_subtree(const subtree_point_t point, const ptrdiff_t extent, tracked_vector<op_container_t>& operators, stack_allocator& stack)
    {
        const auto point_begin_itr = operations.begin() + point.pos;
        const auto point_end_itr = operations.begin() + extent;

        const size_t after_bytes = accumulate_type_sizes(point_end_itr, operations.end());

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
                    auto [_, ptr] = values.access_pointer(for_bytes + after_bytes, it.type_size());
                    ++*ptr;
                }
            }
            operators[operators.size() - 1 - (pos++)] = it;
        }

        stack.copy_from(values, for_bytes, after_bytes);
    }

    void tree_t::swap_subtrees(const subtree_point_t our_subtree, tree_t& other_tree, const subtree_point_t other_subtree)
    {
        const auto c1_subtree_begin_itr = operations.begin() + our_subtree.pos;
        const auto c1_subtree_end_itr = operations.begin() + find_endpoint(our_subtree.pos);

        const auto c2_subtree_begin_itr = other_tree.operations.begin() + other_subtree.pos;
        const auto c2_subtree_end_itr = other_tree.operations.begin() + other_tree.find_endpoint(other_subtree.pos);

        thread_local tracked_vector<op_container_t> c1_subtree_operators;
        thread_local tracked_vector<op_container_t> c2_subtree_operators;
        c1_subtree_operators.clear();
        c2_subtree_operators.clear();

        c1_subtree_operators.reserve(std::distance(c1_subtree_begin_itr, c1_subtree_end_itr));
        c2_subtree_operators.reserve(std::distance(c2_subtree_begin_itr, c2_subtree_end_itr));

        // i don't think this is required for swapping values, since the total number of additions is net zero
        // the tree isn't destroyed at any point.

        size_t c1_subtree_bytes = 0;
        for (const auto& it : iterate(c1_subtree_begin_itr, c1_subtree_end_itr))
        {
            if (it.is_value())
            {
                // if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                // {
                //     auto& ptr = values.access_pointer_forward(for_our_bytes, it.type_size());
                //     ++*ptr;
                // }
                c1_subtree_bytes += it.type_size();
            }
            c1_subtree_operators.push_back(it);
        }

        size_t c2_subtree_bytes = 0;
        for (const auto& it : iterate(c2_subtree_begin_itr, c2_subtree_end_itr))
        {
            if (it.is_value())
            {
                // if (it.get_flags().is_ephemeral() && it.has_ephemeral_drop())
                // {
                //     auto& ptr = values.access_pointer_forward(for_other_bytes, it.type_size());
                //     ++*ptr;
                // }
                c2_subtree_bytes += it.type_size();
            }
            c2_subtree_operators.push_back(it);
        }

        const size_t c1_stack_after_bytes = accumulate_type_sizes(c1_subtree_end_itr, operations.end());
        const size_t c2_stack_after_bytes = accumulate_type_sizes(c2_subtree_end_itr, other_tree.operations.end());
        const auto c1_total = static_cast<ptrdiff_t>(c1_stack_after_bytes + c1_subtree_bytes);
        const auto c2_total = static_cast<ptrdiff_t>(c2_stack_after_bytes + c2_subtree_bytes);
        const auto copy_ptr_c1 = get_thread_pointer_for_size<struct c1_t>(c1_total);
        const auto copy_ptr_c2 = get_thread_pointer_for_size<struct c2_t>(c2_total);

        values.reserve(values.bytes_in_head() - c1_subtree_bytes + c2_subtree_bytes);
        other_tree.values.reserve(other_tree.values.bytes_in_head() - c2_subtree_bytes + c1_subtree_bytes);

        values.copy_to(copy_ptr_c1, c1_total);
        values.pop_bytes(c1_total);

        other_tree.values.copy_to(copy_ptr_c2, c2_total);
        other_tree.values.pop_bytes(c2_total);

        other_tree.values.copy_from(copy_ptr_c1, c1_subtree_bytes);
        other_tree.values.copy_from(copy_ptr_c2 + c2_subtree_bytes, c2_stack_after_bytes);

        values.copy_from(copy_ptr_c2, c2_subtree_bytes);
        values.copy_from(copy_ptr_c1 + c1_subtree_bytes, c1_stack_after_bytes);

        // now swap the operators
        // auto insert_point_c1 = c1_subtree_begin_itr - 1;
        // auto insert_point_c2 = c2_subtree_begin_itr - 1;

        // invalidates [begin, end()) so the insert points should be fine
        auto insert_point_c1 = operations.erase(c1_subtree_begin_itr, c1_subtree_end_itr);
        auto insert_point_c2 = other_tree.operations.erase(c2_subtree_begin_itr, c2_subtree_end_itr);

        operations.insert(insert_point_c1, c2_subtree_operators.begin(), c2_subtree_operators.end());
        other_tree.operations.insert(insert_point_c2, c1_subtree_operators.begin(), c1_subtree_operators.end());
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
                    auto [val, ptr] = values.access_pointer(for_bytes + after_bytes, it.type_size());
                    --*ptr;
                    if (*ptr == 0)
                        handle_ptr_empty(ptr, val, it.id());
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
                    auto [_, pointer] = other_tree.values.access_pointer_forward(copy_bytes, v.type_size());
                    ++*pointer;
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
                    auto [val, ptr] = values.access_pointer(for_bytes + after_bytes, it.type_size());
                    --*ptr;
                    if (*ptr == 0)
                        handle_ptr_empty(ptr, val, it.id());
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
        byte_only_transaction_t transaction{*this, after_bytes};

        auto insert = operations.begin() + point.pos;
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
            insert = operations.insert(insert, it);
        }
        values.insert(other_tree.values);

        return static_cast<ptrdiff_t>(point.pos + other_tree.size());
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
        const auto bytes_size = values.size().total_used_bytes;

        for (const auto& op : operations)
        {
            if (op.is_value())
                bytes_expected += op.type_size();
        }

        if (bytes_expected != bytes_size)
        {
            BLT_ERROR("Stack state: {}", values.size());
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

            const auto v1 = results.values.bytes_in_head();
            const auto v2 = static_cast<ptrdiff_t>(operations.front().type_size());

            // ephemeral don't need to be dropped as there are no copies which matter when checking the tree
            if (!operations.front().get_flags().is_ephemeral())
                m_program->get_destroy_func(operations.front().id())(detail::destroy_t::RETURN, results.values.from(operations.front().type_size()));
            if (v1 != v2)
            {
                const auto vd = std::abs(v1 - v2);
                BLT_ERROR("found {} bytes expected {} bytes, total difference: {}", v1, v2, vd);
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

    tree_t::subtree_point_t tree_t::subtree_from_point(ptrdiff_t point) const
    {
        return {point, m_program->get_operator_info(operations[point].id()).return_type};
    }

    size_t tree_t::required_size() const
    {
        // 2 size_t used to store expected_length of operations + size of the values stack
        return 2 * sizeof(size_t) + operations.size() * sizeof(size_t) + values.bytes_in_head();
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
        const auto val_size = values.bytes_in_head();
        std::memcpy(out, &val_size, sizeof(size_t));
        out += sizeof(size_t);
        std::memcpy(out, values.data(), val_size);
    }

    void tree_t::to_file(FILE* file) const
    {
        const auto op_size = operations.size();
        BLT_ASSERT(std::fwrite(&op_size, sizeof(size_t), 1, file) == sizeof(size_t));
        for (const auto& op : operations)
        {
            auto id = op.id();
            std::fwrite(&id, sizeof(operator_id), 1, file);
        }
        const auto val_size = values.bytes_in_head();
        BLT_ASSERT(std::fwrite(&val_size, sizeof(size_t), 1, file) == sizeof(size_t));
        BLT_ASSERT(std::fwrite(values.data(), val_size, 1, file) == val_size);
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
            operations.push_back({
                m_program->get_typesystem().get_type(m_program->get_operator_info(id).return_type).size(),
                id,
                m_program->is_operator_ephemeral(id),
                m_program->get_operator_flags(id)
            });
        }
        size_t val_size;
        std::memcpy(&val_size, in, sizeof(size_t));
        in += sizeof(size_t);
        // TODO replace instances of u8 that are used to alias types with the proper std::byte
        values.copy_from(reinterpret_cast<const u8*>(in), val_size);
    }

    void tree_t::from_file(FILE* file)
    {
        size_t ops_to_read;
        BLT_ASSERT(std::fread(&ops_to_read, sizeof(size_t), 1, file) == sizeof(size_t));
        operations.reserve(ops_to_read);
        for (size_t i = 0; i < ops_to_read; i++)
        {
            operator_id id;
            BLT_ASSERT(std::fread(&id, sizeof(operator_id), 1, file) == sizeof(operator_id));
            operations.push_back({
                m_program->get_typesystem().get_type(m_program->get_operator_info(id).return_type).size(),
                id,
                m_program->is_operator_ephemeral(id),
                m_program->get_operator_flags(id)
            });
        }
        size_t val_size;
        BLT_ASSERT(std::fread(&val_size, sizeof(size_t), 1, file) == sizeof(size_t));
        values.resize(val_size);
        BLT_ASSERT(std::fread(values.data(), val_size, 1, file) == val_size);
    }

    void tree_t::modify_operator(const size_t point, operator_id new_id, std::optional<type_id> return_type)
    {
        if (!return_type)
            return_type = m_program->get_operator_info(new_id).return_type;
        byte_only_transaction_t move_data{*this};
        if (operations[point].is_value())
        {
            const size_t after_bytes = accumulate_type_sizes(operations.begin() + static_cast<ptrdiff_t>(point) + 1, operations.end());
            move_data.move(after_bytes);
            if (operations[point].get_flags().is_ephemeral() && operations[point].has_ephemeral_drop())
            {
                auto [val, ptr] = values.access_pointer(operations[point].type_size(), operations[point].type_size());
                --*ptr;
                if (*ptr == 0)
                    handle_ptr_empty(ptr, val, operations[point].id());
            }
            values.pop_bytes(operations[point].type_size());
        }
        operations[point] = {
            m_program->get_typesystem().get_type(*return_type).size(),
            new_id,
            m_program->is_operator_ephemeral(new_id),
            m_program->get_operator_flags(new_id)
        };
        if (operations[point].get_flags().is_ephemeral())
        {
            if (move_data.empty())
            {
                const size_t after_bytes = accumulate_type_sizes(operations.begin() + static_cast<ptrdiff_t>(point) + 1, operations.end());
                move_data.move(after_bytes);
            }
            handle_operator_inserted(operations[point]);
        }
    }
}
