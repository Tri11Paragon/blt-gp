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
#include <blt/logging/logging.h>
#include <blt/std/types.h>
#include <ostream>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <blt/std/mmap.h>
#include <blt/gp/util/trackers.h>
#include <blt/gp/allocator.h>

namespace blt::gp
{
    class gp_program;
    
    class type;
    
    struct operator_id;
    
    struct type_id;
    
    struct operator_info_t;
    
    class type_provider;
    
    struct op_container_t;
    
    class evaluation_context;
    
    class tree_t;
    
    struct individual_t;
    
    class population_t;
    
    class tree_generator_t;
    
    class grow_generator_t;
    
    class full_generator_t;
    
    class stack_allocator;
    
    template<typename T>
    class tracked_allocator_t;

    namespace detail
    {
        class operator_storage_test;
        // context*, read stack, write stack
        using operator_func_t = std::function<void(void*, stack_allocator&, stack_allocator&)>;
        using eval_func_t = std::function<evaluation_context&(const tree_t& tree, void* context)>;
        // debug function,
        using print_func_t = std::function<void(std::ostream&, stack_allocator&)>;
        
        enum class destroy_t
        {
            PTR,
            RETURN
        };
        
        using destroy_func_t = std::function<void(destroy_t, u8*)>;
        
        using const_op_iter_t = tracked_vector<op_container_t>::const_iterator;
        using op_iter_t = tracked_vector<op_container_t>::iterator;
    }

#if BLT_DEBUG_LEVEL > 0

    namespace detail::debug
    {
        inline void* context_ptr;
    }

#define BLT_GP_UPDATE_CONTEXT(context) blt::gp::detail::debug::context_ptr = const_cast<void*>(static_cast<const void*>(&context))
#else
    #define BLT_GP_UPDATE_CONTEXT(context)
#endif
    
}

#endif //BLT_GP_FWDECL_H
