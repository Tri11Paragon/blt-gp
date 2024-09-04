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
    
    class aligned_allocator
    {
        public:
            void* allocate(blt::size_t bytes) // NOLINT
            {
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.allocate(bytes);
//                std::cout << "Hey our aligned allocator allocated " << bytes << " bytes!\n";
#endif
                return std::aligned_alloc(8, bytes);
            }
            
            void deallocate(void* ptr, blt::size_t bytes) // NOLINT
            {
                if (ptr == nullptr)
                    return;
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.deallocate(bytes);
//                std::cout << "[Hey our aligned allocator deallocated " << bytes << " bytes!]\n";
#else
                (void) bytes;
#endif
                std::free(ptr);
            }
    };
    
    template<typename Alloc = blt::mmap_huge_allocator>
    class variable_bump_allocator
    {
        public:
            explicit variable_bump_allocator(blt::size_t default_block_size = BLT_2MB_SIZE): default_block_size(default_block_size)
            {}
            
            void* allocate(blt::size_t bytes)
            {
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.allocate(bytes);
#endif
                std::scoped_lock lock(mutex);
                if (head == nullptr || head->remaining_bytes_in_block() < static_cast<blt::ptrdiff_t>(bytes))
                {
                    push_block(bytes);
                }
                auto ptr = head->metadata.offset;
                head->metadata.offset += bytes;
                ++head->metadata.allocated_objects;
                return ptr;
            }
            
            void deallocate(void* ptr, blt::size_t bytes)
            {
                if (ptr == nullptr)
                    return;
#ifdef BLT_TRACK_ALLOCATIONS
                tracker.deallocate(bytes);
#else
                (void) bytes;
#endif
                std::scoped_lock lock(mutex);
                auto blk = to_block(ptr);
                --blk->metadata.allocated_objects;
                if (blk->metadata.allocated_objects == 0)
                {
                    if (head == blk)
                        head = head->metadata.next;
                    else
                    {
                        auto prev = head;
                        auto next = head->metadata.next;
                        while (next != blk)
                        {
                            prev = next;
                            next = next->metadata.next;
                        }
                        prev->metadata.next = next->metadata.next;
                    }
                    deallocated_blocks.push_back(blk);
                }
            }
            
            ~variable_bump_allocator()
            {
                std::scoped_lock lock(mutex);
                for (auto* blk : deallocated_blocks)
                    alloc.deallocate(blk, blk->metadata.size);
                auto cur = head;
                while (cur != nullptr)
                {
                    auto* ptr = cur;
                    cur = cur->metadata.next;
                    alloc.deallocate(ptr, ptr->metadata.size);
                }
                head = nullptr;
            }
        
        private:
            struct block_t
            {
                struct block_metadata_t
                {
                    blt::size_t size = 0;
                    blt::size_t allocated_objects = 0;
                    block_t* next = nullptr;
                    blt::u8* offset = nullptr;
                } metadata;
                blt::u8 buffer[8]{};
                
                explicit block_t(blt::size_t size)
                {
                    metadata.size = size;
                    reset();
                }
                
                void reset()
                {
                    metadata.offset = buffer;
                }
                
                [[nodiscard]] blt::ptrdiff_t storage_size() const noexcept
                {
                    return static_cast<blt::ptrdiff_t>(metadata.size - sizeof(typename block_t::block_metadata_t));
                }
                
                [[nodiscard]] blt::ptrdiff_t used_bytes_in_block() const noexcept
                {
                    return static_cast<blt::ptrdiff_t>(metadata.offset - buffer);
                }
                
                [[nodiscard]] blt::ptrdiff_t remaining_bytes_in_block() const noexcept
                {
                    return storage_size() - used_bytes_in_block();
                }
            };
            
            static inline block_t* to_block(void* p)
            {
                return reinterpret_cast<block_t*>(reinterpret_cast<std::uintptr_t>(p) & static_cast<std::uintptr_t>(~(BLT_2MB_SIZE - 1)));
            }
            
            void push_block(blt::size_t bytes)
            {
                auto blk = allocate_block(bytes);
                blk->metadata.next = head;
                head = blk;
            }
            
            inline block_t* allocate_block(blt::size_t bytes)
            {
                if (!deallocated_blocks.empty())
                {
                    auto blk = deallocated_blocks.back();
                    blk->reset();
                    deallocated_blocks.pop_back();
                    return blk;
                }
                auto size = align_size_to(bytes + sizeof(typename block_t::block_metadata_t), default_block_size);
                auto* ptr = static_cast<block_t*>(alloc.allocate(size, blt::huge_page_t::BLT_2MB_PAGE));
                new(ptr) block_t{size};
                return ptr;
            }
        
        private:
            block_t* head = nullptr;
            std::mutex mutex;
            std::vector<block_t*> deallocated_blocks;
            Alloc alloc;
            blt::size_t default_block_size;
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
//                std::cout << "Hey our tracked allocator allocated " << (n * sizeof(T)) << " bytes!\n";
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
//                std::cout << "[Hey our tracked allocator deallocated " << (n * sizeof(T)) << " bytes!]\n";
#else
                (void) n;
#endif
                std::free(p);
            }
            
            template<class U, class... Args>
            void construct(U* p, Args&& ... args)
            {
                new(p) T(std::forward<Args>(args)...);
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
