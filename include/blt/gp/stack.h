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

#ifndef BLT_GP_STACK_H
#define BLT_GP_STACK_H

#include <memory>
#include <vector>

#include <blt/std/utility.h>

#include "alloc.h"

namespace blt::gp
{
    namespace detail
    {
        // it is very unlikely you need more than 8 byte alignment
        constexpr auto MAX_ALIGNMENT = 8;

        constexpr size_t align_bytes(const size_t size) noexcept
        {
            return (size + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1);
        }

        template<typename T>
        constexpr auto aligned_size = align_bytes(sizeof(std::decay_t<T>));
    }

    /**
     * This class manages a stack of bytes. Any type can be pushed into the stack.
     * Copy operations are not supported.
     * Move operations are supported but do NOT call the stored type's move constructors. It is a single pointer swap.
     */
    template <template<typename, typename> typename Vec = std::vector>
    struct byte_stack_t
    {
        constexpr static size_t PAGE_SIZE = 0x100;

        byte_stack_t() = default;

        explicit byte_stack_t(const size_t reserve)
        {
            this->reserve(reserve);
        }

        byte_stack_t& insert(byte_stack_t& other)
        {
            m_data.insert(m_data.end(), other.m_data.begin(), other.m_data.end());
            other.m_data.clear();
            return *this;
        }

        byte_stack_t& copy(byte_stack_t& other, const size_t from)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + from, other.m_data.end());
            return *this;
        }

        byte_stack_t& copy(byte_stack_t& other, const size_t from, const size_t to)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + from, other.m_data.begin() + to);
            return *this;
        }

        byte_stack_t& copy_back(byte_stack_t& other, const size_t bytes)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + other.sp() - bytes, other.m_data.end());
            return *this;
        }

        byte_stack_t& copy_back(byte_stack_t& other, const size_t bytes, const size_t offset)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + other.sp() - bytes - offset, other.m_data.end() - offset);
            return *this;
        }

        template<typename T>
        byte_stack_t& push(T&& t)
        {
            auto aligned_ptr = get_aligned_pointer(sizeof(std::decay_t<T>));
            new (aligned_ptr) std::decay_t<T>(std::forward<T>(t));
            return *this;
        }

        template<typename T>
        size_t pop_bytes()
        {
            const auto aligned = detail::aligned_size<T>;
            m_data.resize(sp() - aligned);
            return aligned;
        }

        size_t pop_bytes(const size_t bytes)
        {
            const auto aligned = detail::align_bytes(bytes);
            m_data.resize(sp() - aligned);
            return aligned;
        }

        template<typename T>
        T pop()
        {
            if constexpr (std::is_trivially_copyable_v<T>)
            {
                T t {top<T>()};
                pop_bytes<T>();
                return t;
            } else if constexpr (std::is_move_constructible_v<T>)
            {
                T t {std::move(top<T>())};
                pop_bytes<T>();
                return t;
            } else if constexpr (std::is_copy_constructible_v<T>)
            {
                T t {top<T>()};
                pop_bytes<T>();
                return static_cast<const T&>(t);
            } else
            {
                static_assert(std::is_trivially_copyable_v<T> || std::is_move_constructible_v<T> || std::is_copy_constructible_v<T>,
                    "Type must be copyable or movable to use pop. If not, consider using `.top()` or `.pop_bytes()");
            }
            BLT_UNREACHABLE;
        }

        template<typename T>
        T& top()
        {
            return *reinterpret_cast<T*>((m_data.data() + sp()) - detail::aligned_size<T>);
        }

        byte_stack_t& reserve(size_t reserve)
        {
            m_data.reserve(reserve);
            return *this;
        }

        char* data() { return m_data.data(); }
        [[nodiscard]] const char* data() const { return m_data.data(); }
        [[nodiscard]] size_t size() const { return m_data.size(); }
        [[nodiscard]] size_t capacity() const { return m_data.capacity(); }

    private:
        Vec<char, aligned_allocator<char, detail::MAX_ALIGNMENT>> m_data;

        [[nodiscard]] void* get_aligned_pointer(const size_t bytes) noexcept
        {
            const size_t old_stack_ptr = ensure_capacity_resize(bytes);
            return static_cast<void*>(m_data.data() + old_stack_ptr);
        }

        size_t ensure_capacity_resize(const size_t bytes)
        {
            const auto old_sp = sp();
            m_data.resize(old_sp + detail::align_bytes(bytes));
            return old_sp;
        }

        [[nodiscard]] size_t remaining_storage() const
        {
            return capacity() - size();
        }

        [[nodiscard]] size_t sp() const
        {
            return detail::align_bytes(m_data.size());
        }
    };
}

#endif //BLT_GP_STACK_H
