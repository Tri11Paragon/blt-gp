#pragma once
/*
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

#ifndef BLT_GP_FWDECL_H
#define BLT_GP_FWDECL_H

#include <functional>
#include <blt/std/logging.h>
#include <blt/std/types.h>
#include <ostream>

namespace blt::gp
{
    
    class gp_program;
    
    class type;
    
    class type_provider;
    
    struct op_container_t;
    
    class evaluation_context;
    
    class tree_t;
    
    class population_t;
    
    class tree_generator_t;
    
    class grow_generator_t;
    
    class full_generator_t;
    
    class stack_allocator;
    
    namespace detail
    {
        class operator_storage_test;
        // context*, read stack, write stack
        using callable_t = std::function<void(void*, stack_allocator&, stack_allocator&)>;
        // to, from
        using transfer_t = std::function<void(stack_allocator&, stack_allocator&)>;
        // debug function,
        using print_func_t = std::function<void(std::ostream&, stack_allocator&)>;
    }
    
}

#endif //BLT_GP_FWDECL_H
