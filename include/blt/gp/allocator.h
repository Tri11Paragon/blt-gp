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

#ifndef BLT_GP_ALLOCATOR_H
#define BLT_GP_ALLOCATOR_H

#include <blt/std/types.h>
#include <blt/gp/stats.h>
#include <blt/gp/fwdecl.h>
#include <cstdlib>

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

    template <typename T>
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

        template <class U>
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
            ::blt::gp::tracker.deallocate(n * sizeof(T));
            //                std::cout << "[Hey our tracked allocator deallocated " << (n * sizeof(T)) << " bytes!]\n";
#else
                (void) n;
#endif
            std::free(p);
        }

        template <class U, class... Args>
        void construct(U* p, Args&&... args)
        {
            new(p) T(std::forward<Args>(args)...);
        }

        template <class U>
        void destroy(U* p)
        {
            p->~T();
        }

        [[nodiscard]] size_type max_size() const noexcept
        {
            return std::numeric_limits<size_type>::max();
        }
    };

    template <class T1, class T2>
    inline static bool operator==(const tracked_allocator_t<T1>& lhs, const tracked_allocator_t<T2>& rhs) noexcept
    {
        return &lhs == &rhs;
    }

    template <class T1, class T2>
    inline static bool operator!=(const tracked_allocator_t<T1>& lhs, const tracked_allocator_t<T2>& rhs) noexcept
    {
        return &lhs != &rhs;
    }
}

#endif //BLT_GP_ALLOCATOR_H
