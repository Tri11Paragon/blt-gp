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

#include <blt/std/types.h>
#include <blt/std/bump_allocator.h>
#include <blt/std/assert.h>
#include <blt/std/logging.h>
#include <blt/std/allocator.h>
#include <blt/std/ranges.h>
#include <blt/meta/meta.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/util/trackers.h>
#include <blt/gp/allocator.h>
#include <utility>
#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <cstring>
#include <iostream>

namespace blt::gp
{
    namespace detail
    {
        BLT_META_MAKE_FUNCTION_CHECK(drop);
    }

    class stack_allocator
    {
        constexpr static blt::size_t PAGE_SIZE = 0x100;
        template <typename T>
        using NO_REF_T = std::remove_cv_t<std::remove_reference_t<T>>;
        using Allocator = aligned_allocator;

        // todo remove this once i fix all the broken references
        struct detail
        {
            static constexpr size_t aligned_size(const size_t size) noexcept
            {
                return (size + (gp::detail::MAX_ALIGNMENT - 1)) & ~(gp::detail::MAX_ALIGNMENT - 1);
            }
        };

    public:
        static Allocator& get_allocator();

        struct size_data_t
        {
            blt::size_t total_size_bytes = 0;
            blt::size_t total_used_bytes = 0;
            blt::size_t total_remaining_bytes = 0;

            friend std::ostream& operator<<(std::ostream& stream, const size_data_t& data)
            {
                stream << "[";
                stream << data.total_used_bytes << " / " << data.total_size_bytes;
                stream << " ("
                    << (data.total_size_bytes != 0
                            ? (static_cast<double>(data.total_used_bytes) / static_cast<double>(data.total_size_bytes) *
                                100)
                            : 0) << "%); space left: " << data.total_remaining_bytes << "]";
                return stream;
            }
        };

        template <typename T>
        static constexpr size_t aligned_size() noexcept
        {
            const auto bytes = detail::aligned_size(sizeof(NO_REF_T<T>));
            if constexpr (blt::gp::detail::has_func_drop_v<T>)
                return bytes + sizeof(std::atomic_uint64_t*);
            return bytes;
        }

        stack_allocator() = default;

        stack_allocator(const stack_allocator& copy)
        {
            if (copy.data_ == nullptr || copy.bytes_stored == 0)
                return;
            expand(copy.size_);
            std::memcpy(data_, copy.data_, copy.bytes_stored);
            bytes_stored = copy.bytes_stored;
        }

        stack_allocator(stack_allocator&& move) noexcept:
            data_(std::exchange(move.data_, nullptr)), bytes_stored(std::exchange(move.bytes_stored, 0)), size_(std::exchange(move.size_, 0))
        {
        }

        stack_allocator& operator=(const stack_allocator& copy) = delete;

        stack_allocator& operator=(stack_allocator&& move) noexcept
        {
            data_ = std::exchange(move.data_, data_);
            size_ = std::exchange(move.size_, size_);
            bytes_stored = std::exchange(move.bytes_stored, bytes_stored);
            return *this;
        }

        ~stack_allocator()
        {
            get_allocator().deallocate(data_, size_);
        }

        void insert(const stack_allocator& stack)
        {
            if (stack.empty())
                return;
            if (stack.bytes_stored + bytes_stored > size_)
                expand(stack.bytes_stored + size_);
            std::memcpy(data_ + bytes_stored, stack.data_, stack.bytes_stored);
            bytes_stored += stack.bytes_stored;
        }

        void copy_from(const stack_allocator& stack, blt::size_t bytes)
        {
            if (bytes == 0)
                return;
            if (bytes + bytes_stored > size_)
                expand(bytes + size_);
            std::memcpy(data_ + bytes_stored, stack.data_ + (stack.bytes_stored - bytes), bytes);
            bytes_stored += bytes;
        }

        void copy_from(const u8* data, const size_t bytes)
        {
            if (bytes == 0 || data == nullptr)
                return;
            if (bytes + bytes_stored > size_)
                expand(bytes + size_);
            std::memcpy(data_ + bytes_stored, data, bytes);
            bytes_stored += bytes;
        }

        void copy_to(u8* data, const size_t bytes) const
        {
            if (bytes == 0 || data == nullptr)
                return;
            std::memcpy(data, data_ + (bytes_stored - bytes), bytes);
        }

        template <typename T, typename NO_REF = NO_REF_T<T>>
        void push(const T& t)
        {
            static_assert(std::is_trivially_copyable_v<NO_REF>, "Type must be bitwise copyable!");
            static_assert(alignof(NO_REF) <= gp::detail::MAX_ALIGNMENT, "Type alignment must not be greater than the max alignment!");
            const auto ptr = static_cast<char*>(allocate_bytes_for_size(aligned_size<NO_REF>()));
            std::memcpy(ptr, &t, sizeof(NO_REF));
            // what about non ephemeral values?
            if constexpr (gp::detail::has_func_drop_v<T>)
            {
                BLT_TRACE("Hello!");
                const auto* ref_counter_ptr = new std::atomic_uint64_t(1); // NOLINT
                std::memcpy(ptr + sizeof(NO_REF), &ref_counter_ptr, sizeof(std::atomic_uint64_t*));
            }
        }

        template <typename T, typename NO_REF = NO_REF_T<T>>
        T pop()
        {
            static_assert(std::is_trivially_copyable_v<NO_REF>, "Type must be bitwise copyable!");
            static_assert(alignof(NO_REF) <= gp::detail::MAX_ALIGNMENT, "Type alignment must not be greater than the max alignment!");
            constexpr auto size = aligned_size<NO_REF>();
#if BLT_DEBUG_LEVEL > 0
            if (bytes_stored < size)
                BLT_ABORT("Not enough bytes left to pop!");
#endif
            bytes_stored -= size;
            return *reinterpret_cast<T*>(data_ + bytes_stored);
        }

        [[nodiscard]] u8* from(const size_t bytes) const
        {
#if BLT_DEBUG_LEVEL > 0
            if (bytes_stored < bytes)
                BLT_ABORT(("Not enough bytes in stack to reference " + std::to_string(bytes) + " bytes requested but " + std::to_string(bytes) +
                " bytes stored!").c_str());
#endif
            return data_ + (bytes_stored - bytes);
        }

        template <typename T, typename NO_REF = NO_REF_T<T>>
        T& from(const size_t bytes)
        {
            static_assert(std::is_trivially_copyable_v<NO_REF> && "Type must be bitwise copyable!");
            static_assert(alignof(NO_REF) <= gp::detail::MAX_ALIGNMENT && "Type alignment must not be greater than the max alignment!");
            return *reinterpret_cast<NO_REF*>(from(aligned_size<NO_REF>() + bytes));
        }

        void pop_bytes(const size_t bytes)
        {
#if BLT_DEBUG_LEVEL > 0
            if (bytes_stored < bytes)
                BLT_ABORT(("Not enough bytes in stack to pop " + std::to_string(bytes) + " bytes requested but " + std::to_string(bytes) +
                " bytes stored!").c_str());
            gp::detail::check_alignment(bytes);
#endif
            bytes_stored -= bytes;
        }

        void transfer_bytes(stack_allocator& to, const size_t aligned_bytes)
        {
#if BLT_DEBUG_LEVEL > 0
            if (bytes_stored < aligned_bytes)
                BLT_ABORT(("Not enough bytes in stack to transfer " + std::to_string(aligned_bytes) + " bytes requested but " + std::to_string(aligned_bytes) +
                " bytes stored!").c_str());
            gp::detail::check_alignment(aligned_bytes);
#endif
            to.copy_from(*this, aligned_bytes);
            pop_bytes(aligned_bytes);
        }

        template <typename... Args>
        void call_destructors()
        {
            if constexpr (sizeof...(Args) > 0)
            {
                size_t offset = (aligned_size<NO_REF_T<Args>>() + ...) - aligned_size<NO_REF_T<typename meta::arg_helper<Args...>::First>>();
                ((call_drop<Args>(offset + (gp::detail::has_func_drop_v<Args> ? sizeof(u64*) : 0)), offset -= aligned_size<NO_REF_T<Args>>()), ...);
                (void) offset;
            }
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return bytes_stored == 0;
        }

        [[nodiscard]] ptrdiff_t remaining_bytes_in_block() const noexcept
        {
            return static_cast<ptrdiff_t>(size_ - bytes_stored);
        }

        [[nodiscard]] ptrdiff_t bytes_in_head() const noexcept
        {
            return static_cast<ptrdiff_t>(bytes_stored);
        }

        [[nodiscard]] size_data_t size() const noexcept
        {
            size_data_t data;

            data.total_used_bytes = bytes_stored;
            data.total_size_bytes = size_;
            data.total_remaining_bytes = remaining_bytes_in_block();

            return data;
        }

        void reserve(const size_t bytes)
        {
            if (bytes > size_)
                expand_raw(bytes);
        }

        [[nodiscard]] size_t stored() const
        {
            return bytes_stored;
        }

        [[nodiscard]] size_t internal_storage_size() const
        {
            return size_;
        }

        void reset()
        {
            bytes_stored = 0;
        }

    private:
        void expand(const size_t bytes)
        {
            //bytes = to_nearest_page_size(bytes);
            expand_raw(bytes);
        }

        void expand_raw(const size_t bytes)
        {
            // auto aligned = detail::aligned_size(bytes);
            const auto new_data = static_cast<u8*>(get_allocator().allocate(bytes));
            if (bytes_stored > 0)
                std::memcpy(new_data, data_, bytes_stored);
            get_allocator().deallocate(data_, size_);
            data_ = new_data;
            size_ = bytes;
        }

        static size_t to_nearest_page_size(const size_t bytes) noexcept
        {
            constexpr static size_t MASK = ~(PAGE_SIZE - 1);
            return (bytes & MASK) + PAGE_SIZE;
        }

        [[nodiscard]] void* get_aligned_pointer(const size_t bytes) const noexcept
        {
            if (data_ == nullptr)
                return nullptr;
            size_t remaining_bytes = remaining_bytes_in_block();
            auto* pointer = static_cast<void*>(data_ + bytes_stored);
            return std::align(gp::detail::MAX_ALIGNMENT, bytes, pointer, remaining_bytes);
        }

        void* allocate_bytes_for_size(const size_t aligned_bytes)
        {
#if BLT_DEBUG_LEVEL > 0
            gp::detail::check_alignment(aligned_bytes);
#endif
            auto aligned_ptr = get_aligned_pointer(aligned_bytes);
            if (aligned_ptr == nullptr)
            {
                expand(size_ + aligned_bytes);
                aligned_ptr = get_aligned_pointer(aligned_bytes);
            }
            if (aligned_ptr == nullptr)
                throw std::bad_alloc();
            bytes_stored += aligned_bytes;
            return aligned_ptr;
        }

        template <typename T>
        void call_drop(const size_t offset)
        {
            if constexpr (blt::gp::detail::has_func_drop_v<T>)
            {
                from<NO_REF_T<T>>(offset).drop();
            }
        }

        u8* data_ = nullptr;
        // place in the data_ array which has a free spot.
        size_t bytes_stored = 0;
        size_t size_ = 0;
    };

    template <size_t Size>
    struct ref_counted_type
    {
        explicit ref_counted_type(size_t* ref_count): ref_count(ref_count)
        {
        }

        size_t* ref_count = nullptr;
        u8 storage[Size]{};

        static size_t* access(const void* ptr)
        {
            ref_counted_type<1> type{nullptr};
            std::memcpy(&type, ptr, sizeof(size_t*));
            return type.ref_count;
        }
    };
}

#endif //BLT_GP_STACK_H
