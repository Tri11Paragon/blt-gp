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

#include <blt/gp/util/traits.h>
#include <blt/gp/alloc.h>

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

        template<typename...>
        struct offsets_of
        {
            constexpr static size_t align_size = 0;
            constexpr static auto offset_tuple = std::make_tuple();
        };

        template<typename First, typename... Args>
        struct offsets_of<First, Args...>
        {
            // aligned size of all template arguments together
            constexpr static size_t align_size = aligned_size<First> + offsets_of<Args...>::align_size;
            // offset of the first template argument
            constexpr static size_t offset = align_size - aligned_size<First>;

            // offsets of all template arguments
            constexpr static auto offset_tuple = std::tuple_cat(std::make_tuple(offset), offsets_of<Args...>::offset_tuple);

            using args = std::tuple<First, Args...>;
        };

        template<typename... Args>
        struct apply_offset
        {

        };
    }

    /**
     * This class manages a stack of bytes.
     */
    template <template<typename...> typename Vec = std::vector>
    struct byte_stack_t
    {
        constexpr static size_t PAGE_SIZE = 0x100;

        byte_stack_t() = default;

        explicit byte_stack_t(const size_t reserve)
        {
            this->reserve(reserve);
        }

        /**
         * Inserts another byte stack to the end of this byte stack.
         */
        byte_stack_t& insert(byte_stack_t& other)
        {
            m_data.insert(m_data.end(), other.m_data.begin(), other.m_data.end());
            other.m_data.clear();
            return *this;
        }

        /**
         * Replaces a range in this byte stack with the complete range from the supplied other byte stack.
         */
        byte_stack_t& replace(const byte_stack_t& other, const size_t from, const size_t to)
        {
            m_data.erase(m_data.begin() + from, m_data.begin() + to);
            m_data.insert(m_data.begin() + from, other.m_data.begin(), other.m_data.end());
            return *this;
        }

        byte_stack_t& erase(const size_t from, const size_t to)
        {
            m_data.erase(m_data.begin() + from, m_data.begin() + to);
            return *this;
        }

        byte_stack_t& copy(const byte_stack_t& other, const size_t from)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + from, other.m_data.end());
            return *this;
        }

        byte_stack_t& copy(const byte_stack_t& other, const size_t from, const size_t to)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + from, other.m_data.begin() + to);
            return *this;
        }

        byte_stack_t& copy_back(const byte_stack_t& other, const size_t bytes)
        {
            m_data.insert(m_data.end(), other.m_data.begin() + other.sp() - bytes, other.m_data.end());
            return *this;
        }

        byte_stack_t& copy_back(const byte_stack_t& other, const size_t bytes, const size_t offset)
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
            static_assert(std::is_trivially_copyable_v<T>, "Types must be trivially copyable");
            T t {top<T>()};
            pop_bytes<T>();
            return t;
        }

        template<typename T, typename U = std::decay_t<T>>
        U& top(const size_t offset = 0)
        {
            return *reinterpret_cast<U*>((m_data.data() + sp() - offset) - detail::aligned_size<U>);
        }

        template<typename T, typename U = std::decay_t<T>>
        const U& top(const size_t offset = 0) const
        {
            return *reinterpret_cast<const U*>((m_data.data() + sp() - offset) - detail::aligned_size<U>);
        }

        /**
         * Calls the function with the function argument's taken from the stack.
         * Arguments are passed as references or const references depending on the constness of the stack.
         *
         * The last argument to the function is the last argument pushed to the stack.
         * The other types are taken from the stack in reverse order, following such logic.
         *
         * Alignment and offset of the values stored in the stack are managed by this stack.
         * No automatic type conversion is allowed. Types stored must be valid for the arguments of the function call.
         */
        template<typename Func>
        auto call_with_arguments(Func& func) -> decltype(auto)
        {
            using Reflect = util::function_traits<std::decay_t<Func>>;
            return call_with_args_internal(func, std::make_index_sequence<Reflect::arity>{});
        }

        template<typename Func>
        auto call_with_arguments(Func& func) const -> decltype(auto)
        {
            using Reflect = util::function_traits<std::decay_t<Func>>;
            return call_with_args_internal(func, std::make_index_sequence<Reflect::arity>{});
        }

        byte_stack_t& reserve(size_t reserve)
        {
            m_data.reserve(reserve);
            return *this;
        }

        void clear() { m_data.clear(); }
        char* data() { return m_data.data(); }
        [[nodiscard]] const char* data() const { return m_data.data(); }
        [[nodiscard]] size_t size() const { return m_data.size(); }
        [[nodiscard]] size_t capacity() const { return m_data.capacity(); }
        [[nodiscard]] bool empty() const { return m_data.empty(); }

    private:
        Vec<char, aligned_allocator<char, detail::MAX_ALIGNMENT>> m_data;

        template<typename Func, size_t... Is>
        auto call_with_args_internal(Func& func, std::index_sequence<Is...>) -> decltype(auto)
        {
            using Reflect = util::function_traits<std::decay_t<Func>>;
            constexpr auto tuple = util::apply_t<detail::offsets_of, typename Reflect::args_tuple>::offset_tuple;
            static_assert(std::tuple_size_v<decltype(tuple)> == Reflect::arity, "arity mismatch");
            return func(top<std::tuple_element_t<Is, typename Reflect::args_tuple>>(std::get<Is>(tuple))...);
        }

        template<typename Func, size_t... Is>
        auto call_with_args_internal(Func& func, std::index_sequence<Is...>) const -> decltype(auto)
        {
            using Reflect = util::function_traits<std::decay_t<Func>>;
            constexpr auto tuple = util::apply_t<detail::offsets_of, typename Reflect::args_tuple>::offset_tuple;
            static_assert(std::tuple_size_v<decltype(tuple)> == Reflect::arity, "arity mismatch");
            return func(top<std::tuple_element_t<Is, typename Reflect::args_tuple>>(std::get<Is>(tuple))...);
        }

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
