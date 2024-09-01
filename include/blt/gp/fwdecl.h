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

namespace blt::gp
{
    inline allocation_tracker_t tracker;
    
    class gp_program;
    
    class type;
    
    struct operator_id;
    
    struct type_id;
    
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
    
    template<typename T>
    using tracked_vector = std::vector<T, tracked_allocator_t<T>>;
    
//    using operation_vector_t = tracked_vector<op_container_t>;
//    using individual_vector_t = tracked_vector<individual_t, tracked_allocator_t<individual_t>>;
//    using tree_vector_t = tracked_vector<tree_t>;
    
    class aligned_allocator
    {
        public:
            void* allocate(blt::size_t bytes) // NOLINT
            {
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.allocate(bytes);
#endif
                return std::aligned_alloc(8, bytes);
            }
            
            void deallocate(void* ptr, blt::size_t bytes) // NOLINT
            {
                if (ptr == nullptr)
                    return;
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.deallocate(bytes);
#else
                (void) bytes;
#endif
                std::free(ptr);
            }
    };
    
    template<typename T>
    class tracked_allocator_t
    {
        public:
            using value_type = T;
            using reference = T&;
            using const_reference = const T&;
            using pointer = T*;
            using const_pointer = const T*;
            using void_pointer = void*;
            using const_void_pointer = const void*;
            using difference_type = blt::ptrdiff_t;
            using size_type = blt::size_t;
            template<class U>
            struct rebind
            {
                typedef tracked_allocator_t<U> other;
            };
            
            pointer allocate(size_type n)
            {
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.allocate(n * sizeof(T));
#endif
                return static_cast<pointer>(std::malloc(n * sizeof(T)));
            }
            
            pointer allocate(size_type n, const_void_pointer)
            {
                return allocate(n);
            }
            
            void deallocate(pointer p, size_type n)
            {
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.deallocate(n * sizeof(T));
#else
                (void) n;
#endif
                std::free(p);
            }
            
            template<class U, class... Args>
            void construct(U* p, Args&& ... args)
            {
                new(p) T(args...);
            }
            
            template<class U>
            void destroy(U* p)
            {
                p->~T();
            }
            
            [[nodiscard]] size_type max_size() const noexcept
            {
                return std::numeric_limits<size_type>::max();
            }
    };
    
    template<class T1, class T2>
    inline static bool operator==(const tracked_allocator_t<T1>& lhs, const tracked_allocator_t<T2>& rhs) noexcept
    {
        return &lhs == &rhs;
    }
    
    template<class T1, class T2>
    inline static bool operator!=(const tracked_allocator_t<T1>& lhs, const tracked_allocator_t<T2>& rhs) noexcept
    {
        return &lhs != &rhs;
    }
    
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
