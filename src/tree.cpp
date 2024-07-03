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
}