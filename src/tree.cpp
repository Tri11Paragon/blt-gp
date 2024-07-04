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
    inline auto empty_callable = detail::callable_t([](void*, stack_allocator&, stack_allocator&) { BLT_ABORT("This should never be called!"); });
    
    evaluation_context tree_t::evaluate(void* context)
    {
        // copy the initial values
        evaluation_context results{};
        
        auto value_stack = values;
        auto& values_process = results.values;
        auto operations_stack = operations;
        
        while (!operations_stack.empty())
        {
            auto operation = operations_stack.back();
            // keep the last value in the stack on the process stack stored in the eval context, this way it can be accessed easily.
            operations_stack.pop_back();
            if (operation.is_value)
            {
                operation.transfer(values_process, value_stack);
                continue;
            }
            operation.func(context, values_process, value_stack);
            operations_stack.emplace_back(empty_callable, operation.transfer, operation.id, true);
        }
        
        return results;
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
    
    void tree_t::print(gp_program& program, std::ostream& out, bool print_literals, bool pretty_print, bool include_types)
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
                    v.transfer(reversed, copy);
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
                    program.get_print_func(v.id)(out, reversed);
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
}