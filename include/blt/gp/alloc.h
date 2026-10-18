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

#ifndef BLT_GP_ALLOC_H
#define BLT_GP_ALLOC_H

#include <cstddef>
#include <new>
#include <limits>

template <typename T, std::size_t Alignment>
struct aligned_allocator
{
    using value_type = T;

    aligned_allocator() noexcept = default;

    template <typename U>
    aligned_allocator(const aligned_allocator<U, Alignment>&) noexcept
    {
    }

    [[nodiscard]] T* allocate(const std::size_t n)
    {
        if (n > std::numeric_limits<std::size_t>::max() / sizeof(T))
            throw std::bad_array_new_length{};

        void* ptr = ::operator new(
            n * sizeof(T),
            std::align_val_t{Alignment}
        );

        return static_cast<T*>(ptr);
    }

    void deallocate(T* ptr, std::size_t) noexcept
    {
        ::operator delete(
            ptr,
            std::align_val_t{Alignment}
        );
    }

    template <typename U>
    struct rebind
    {
        using other = aligned_allocator<U, Alignment>;
    };
};

template <typename T, typename U, std::size_t Alignment>
bool operator==(
    const aligned_allocator<T, Alignment>&,
    const aligned_allocator<U, Alignment>&
) noexcept
{
    return true;
}

template <typename T, typename U, std::size_t Alignment>
bool operator!=(
    const aligned_allocator<T, Alignment>&,
    const aligned_allocator<U, Alignment>&
) noexcept
{
    return false;
}

#endif //BLT_GP_ALLOC_H
