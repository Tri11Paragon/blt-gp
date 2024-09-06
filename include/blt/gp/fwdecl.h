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
#include <blt/gp/stats.h>
#include <ostream>
#include <cstdlib>
#include <mutex>
#include <atomic>
#include <blt/std/mmap.h>
#include <blt/gp/allocator.h>

namespace blt::gp
{
#ifdef BLT_TRACK_ALLOCATIONS
    inline allocation_tracker_t tracker;
    
    // population gen specifics
    inline call_tracker_t crossover_calls;
    inline call_tracker_t mutation_calls;
    inline call_tracker_t reproduction_calls;
    inline call_tracker_t crossover_allocations;
    inline call_tracker_t mutation_allocations;
    inline call_tracker_t reproduction_allocations;
    
    // for evaluating fitness
    inline call_tracker_t evaluation_calls;
    inline call_tracker_t evaluation_allocations;
#endif
    
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

#ifdef BLT_TRACK_ALLOCATIONS
    template<typename T>
    using tracked_vector = std::vector<T, tracked_allocator_t<T>>;
#else
    template<typename T>
    using tracked_vector = std::vector<T>;
#endif

//    using operation_vector_t = tracked_vector<op_container_t>;
//    using individual_vector_t = tracked_vector<individual_t, tracked_allocator_t<individual_t>>;
//    using tree_vector_t = tracked_vector<tree_t>;
    
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
            ARGS,
            RETURN
        };
        
        using destroy_func_t = std::function<void(destroy_t, stack_allocator&)>;
        
        using const_op_iter_t = tracked_vector<op_container_t>::const_iterator;
        using op_iter_t = tracked_vector<op_container_t>::iterator;
    }
    
}

#endif //BLT_GP_FWDECL_H
