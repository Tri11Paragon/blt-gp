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
    // this one will copy previous bytes over
    template<typename T>
    blt::span<blt::u8> get_pointer_for_size(blt::size_t size)
    {
        static blt::span<blt::u8> buffer{nullptr, 0};
        if (buffer.size() < size)
        {
            delete[] buffer.data();
            buffer = {new blt::u8[size], size};
        }
        return buffer;
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
    
    void tree_t::print(gp_program& program, std::ostream& out, bool print_literals, bool pretty_print, bool include_types) const
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
                if (v.is_value)
                    copy.transfer_bytes(reversed, v.type_size);
            }
        }
        for (const auto& v : operations)
        {
            auto info = program.get_operator_info(v.id);
            auto name = program.get_name(v.id) ? program.get_name(v.id).value() : "NULL";
            auto return_type = get_return_type(program, info.return_type, include_types);
            if (info.argc.argc > 0)
            {
                create_indent(out, indent, pretty_print) << "(";
                indent++;
                arguments_left.emplace(info.argc.argc);
                out << name << return_type << end_indent(pretty_print);
            } else
            {
                if (print_literals)
                {
                    create_indent(out, indent, pretty_print);
                    if (program.is_operator_ephemeral(v.id))
                    {
                        program.get_print_func(v.id)(out, reversed);
                        reversed.pop_bytes(stack_allocator::aligned_size(v.type_size));
                    } else
                        out << name;
                    out << return_type << end_indent(pretty_print);
                } else
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
                } else
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
            } else
            {
                BLT_ERROR("Failed to print tree correctly!");
                break;
            }
        }
        
        out << '\n';
    }
    
    blt::size_t tree_t::get_depth(gp_program& program)
    {
        blt::size_t depth = 0;
        
        auto operations_stack = operations;
        std::vector<blt::size_t> values_process;
        std::vector<blt::size_t> value_stack;
        
        for (const auto& op : operations_stack)
        {
            if (op.is_value)
                value_stack.push_back(1);
        }
        
        while (!operations_stack.empty())
        {
            auto operation = operations_stack.back();
            // keep the last value in the stack on the process stack stored in the eval context, this way it can be accessed easily.
            operations_stack.pop_back();
            if (operation.is_value)
            {
                auto d = value_stack.back();
                depth = std::max(depth, d);
                values_process.push_back(d);
                value_stack.pop_back();
                continue;
            }
            blt::size_t local_depth = 0;
            for (blt::size_t i = 0; i < program.get_operator_info(operation.id).argc.argc; i++)
            {
                local_depth = std::max(local_depth, values_process.back());
                values_process.pop_back();
            }
            value_stack.push_back(local_depth + 1);
            operations_stack.emplace_back(operation.type_size, operation.id, true);
        }
        
        return depth;
    }
    
    blt::ptrdiff_t tree_t::find_endpoint(gp_program& program, blt::ptrdiff_t index) const
    {
        blt::i64 children_left = 0;
        
        do
        {
            const auto& type = program.get_operator_info(operations[index].id);
            // this is a child to someone
            if (children_left != 0)
                children_left--;
            if (type.argc.argc > 0)
                children_left += type.argc.argc;
            index++;
        } while (children_left > 0);
        
        return index;
    }
    
    // this function doesn't work!
    blt::ptrdiff_t tree_t::find_parent(gp_program& program, blt::ptrdiff_t index) const
    {
        blt::i64 children_left = 0;
        do
        {
            if (index == 0)
                return 0;
            const auto& type = program.get_operator_info(operations[index].id);
            if (type.argc.argc > 0)
                children_left -= type.argc.argc;
            children_left++;
            if (children_left <= 0)
                break;
            --index;
        } while (true);
        
        return index;
    }
    
    bool tree_t::check(gp_program& program, void* context) const
    {
        blt::size_t bytes_expected = 0;
        auto bytes_size = values.size().total_used_bytes;
        
        for (const auto& op : get_operations())
        {
            if (op.is_value)
                bytes_expected += stack_allocator::aligned_size(op.type_size);
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
        
        blt::size_t total_produced = 0;
        blt::size_t total_consumed = 0;

        for (const auto& operation : blt::reverse_iterate(operations.begin(), operations.end()))
        {
            if (operation.is_value)
            {
                value_stack.transfer_bytes(values_process, operation.type_size);
                total_produced += stack_allocator::aligned_size(operation.type_size);
                continue;
            }
            auto& info = program.get_operator_info(operation.id);
            for (auto& arg : info.argument_types)
                total_consumed += stack_allocator::aligned_size(program.get_typesystem().get_type(arg).size());
            program.get_operator_info(operation.id).func(context, values_process, values_process);
            total_produced += stack_allocator::aligned_size(program.get_typesystem().get_type(info.return_type).size());
        }
        
        auto v1 = results.values.bytes_in_head();
        auto v2 = static_cast<blt::ptrdiff_t>(stack_allocator::aligned_size(operations.front().type_size));
        if (v1 != v2)
        {
            auto vd = std::abs(v1 - v2);
            BLT_ERROR("found %ld bytes expected %ld bytes, total difference: %ld", v1, v2, vd);
            BLT_ERROR("Total Produced %ld || Total Consumed %ld || Total Difference %ld", total_produced, total_consumed,
                      std::abs(static_cast<blt::ptrdiff_t>(total_produced) - static_cast<blt::ptrdiff_t>(total_consumed)));
            return false;
        }
        return true;
    }
    
    void tree_t::find_child_extends(gp_program& program, std::vector<child_t>& vec, blt::size_t parent_node, blt::size_t argc) const
    {
        while (vec.size() < argc)
        {
            auto current_point = vec.size();
            child_t prev{};
            if (current_point == 0)
            {
                // first child.
                prev = {static_cast<blt::ptrdiff_t>(parent_node + 1),
                        find_endpoint(program, static_cast<blt::ptrdiff_t>(parent_node + 1))};
                vec.push_back(prev);
                continue;
            } else
                prev = vec[current_point - 1];
            child_t next = {prev.end, find_endpoint(program, prev.end)};
            vec.push_back(next);
        }
    }
    
    tree_t::tree_t(gp_program& program): func(&program.get_eval_func())
    {
    
    }
    
    void tree_t::clear(gp_program& program)
    {
        auto* f = &program.get_eval_func();
        if (f != func)
            func = f;
        operations.clear();
        values.reset();
    }
}