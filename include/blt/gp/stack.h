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
#include <blt/logging/logging.h>
#include <blt/std/allocator.h>
#include <blt/meta/meta.h>
#include <blt/gp/util/meta.h>
#include <blt/gp/allocator.h>
#include <utility>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <cstring>

namespace blt::gp
{
    namespace detail
    {
        BLT_META_MAKE_FUNCTION_CHECK(drop);
    }

    /**
     * @brief This is the primary class that enables a type-erased GP system without compromising on performance.
     *
     * This class provides an efficient way to allocate, deallocate, and manage memory blocks
     * in a stack-like structure. It supports operations like memory alignment, copying, moving,
     * insertion, and removal of memory. This is particularly useful for performance-critical
     * systems requiring temporary memory management without frequent heap allocation overhead.
     *
     * Types placed within this container cannot have an alignment greater than `BLT_GP_MAX_ALIGNMENT` bytes, doing so will result in unaligned pointer access.
     * You can configure this by setting `BLT_GP_MAX_ALIGNMENT` as a compiler definition but be aware it will increase memory requirements.
     * Setting `BLT_GP_MAX_ALIGNMENT` to lower than 8 is UB on x86-64 systems.
     * Consequently, all types have a minimum storage size of `BLT_GP_MAX_ALIGNMENT` (8) bytes, meaning a char, float, int, etc. will take `BLT_GP_MAX_ALIGNMENT` bytes
     */
    class stack_allocator
    {
        constexpr static size_t PAGE_SIZE = 0x100;
        using Allocator = aligned_allocator;

        static constexpr size_t align_bytes(const size_t size) noexcept
        {
            return (size + (detail::MAX_ALIGNMENT - 1)) & ~(detail::MAX_ALIGNMENT - 1);
        }

    public:
        static Allocator& get_allocator();

        template <typename T>
        static constexpr size_t aligned_size() noexcept
        {
            const auto bytes = align_bytes(sizeof(std::decay_t<T>));
            if constexpr (blt::gp::detail::has_func_drop_v<detail::remove_cv_ref<T>>)
                return bytes + align_bytes(sizeof(std::atomic_uint64_t*));
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

        void copy_from(const stack_allocator& stack, const size_t bytes)
        {
            // TODO: add debug checks to these functions! (check for out of bounds copy)
            if (bytes == 0)
                return;
            if (bytes + bytes_stored > size_)
                expand(bytes + size_);
            std::memcpy(data_ + bytes_stored, stack.data_ + (stack.bytes_stored - bytes), bytes);
            bytes_stored += bytes;
        }

        void copy_from(const stack_allocator& stack, const size_t bytes, const size_t offset)
        {
            if (bytes == 0)
                return;
            if (bytes + bytes_stored > size_)
                expand(bytes + size_);
            std::memcpy(data_ + bytes_stored, stack.data_ + (stack.bytes_stored - bytes - offset), bytes);
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

        template <typename T>
        void push(const T& t)
        {
            using DecayedT = std::decay_t<T>;
            static_assert(std::is_trivially_copyable_v<DecayedT>, "Type must be bitwise copyable!");
            static_assert(alignof(DecayedT) <= detail::MAX_ALIGNMENT, "Type alignment must not be greater than the max alignment!");
            const auto ptr = static_cast<char*>(allocate_bytes_for_size(aligned_size<DecayedT>()));
            std::memcpy(ptr, &t, sizeof(DecayedT));

            if constexpr (gp::detail::has_func_drop_v<detail::remove_cv_ref<T>>)
            {
                new(ptr + sizeof(DecayedT)) mem::pointer_storage<std::atomic_uint64_t>{nullptr};
            }
        }

        template <typename T>
        T pop()
        {
            using DecayedT = std::decay_t<T>;
            static_assert(std::is_trivially_copyable_v<DecayedT>, "Type must be bitwise copyable!");
            static_assert(alignof(DecayedT) <= detail::MAX_ALIGNMENT, "Type alignment must not be greater than the max alignment!");
            constexpr auto size = aligned_size<DecayedT>();
#if BLT_DEBUG_LEVEL > 0
            if (bytes_stored < size)
                throw std::runtime_error(("Not enough bytes left to pop!" __FILE__ ":") + std::to_string(__LINE__));
#endif
            bytes_stored -= size;
            return *reinterpret_cast<T*>(data_ + bytes_stored);
        }

        [[nodiscard]] u8* from(const size_t bytes) const
        {
#if BLT_DEBUG_LEVEL > 0
            if (bytes_stored < bytes)
                throw std::runtime_error(
                    "Not enough bytes in stack! " + std::to_string(bytes) + " bytes requested but only " + std::to_string(bytes_stored) +
                    (" bytes stored! (at " __FILE__ ":") + std::to_string(__LINE__));
#endif
            return data_ + (bytes_stored - bytes);
        }

        template <typename T>
        T& from(const size_t bytes) const
        {
            using DecayedT = std::decay_t<T>;
            static_assert(std::is_trivially_copyable_v<DecayedT> && "Type must be bitwise copyable!");
            static_assert(alignof(DecayedT) <= detail::MAX_ALIGNMENT && "Type alignment must not be greater than the max alignment!");
            return *reinterpret_cast<DecayedT*>(from(aligned_size<DecayedT>() + bytes));
        }

        [[nodiscard]] std::pair<u8*, mem::pointer_storage<std::atomic_uint64_t>&> access_pointer(const size_t bytes, const size_t type_size) const
        {
            const auto type_ref = from(bytes);
            return {
                type_ref, *std::launder(
                    reinterpret_cast<mem::pointer_storage<std::atomic_uint64_t>*>(type_ref + (type_size - align_bytes(
                        sizeof(std::atomic_uint64_t*)))))
            };
        }

        [[nodiscard]] std::pair<u8*, mem::pointer_storage<std::atomic_uint64_t>&> access_pointer_forward(
            const size_t bytes, const size_t type_size) const
        {
            const auto type_ref = data_ + bytes;
            return {
                type_ref, *std::launder(
                    reinterpret_cast<mem::pointer_storage<std::atomic_uint64_t>*>(type_ref + (type_size - align_bytes(
                        sizeof(std::atomic_uint64_t*)))))
            };
        }

        template <typename T>
        [[nodiscard]] std::pair<T&, mem::pointer_storage<std::atomic_uint64_t>&> access_pointer(const size_t bytes) const
        {
            auto& type_ref = from<T>(bytes);
            return {
                type_ref, *std::launder(
                    reinterpret_cast<mem::pointer_storage<std::atomic_uint64_t>*>(reinterpret_cast<char*>(&type_ref) +
                        align_bytes(sizeof(T))))
            };
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
                BLT_ABORT(
                ("Not enough bytes in stack to transfer " + std::to_string(aligned_bytes) + " bytes requested but " + std::to_string(aligned_bytes) +
                    " bytes stored!").c_str());
            gp::detail::check_alignment(aligned_bytes);
#endif
            to.copy_from(*this, aligned_bytes);
            pop_bytes(aligned_bytes);
        }

        [[nodiscard]] bool empty() const noexcept
        {
            return bytes_stored == 0;
        }

        [[nodiscard]] ptrdiff_t remainder() const noexcept
        {
            return static_cast<ptrdiff_t>(size_ - bytes_stored);
        }

        [[nodiscard]] size_t stored() const noexcept
        {
            return bytes_stored;
        }

        void reserve(const size_t bytes)
        {
            if (bytes > size_)
                expand_raw(bytes);
        }

        void resize(const size_t bytes)
        {
            reserve(bytes);
            bytes_stored = bytes;
        }

        [[nodiscard]] size_t capacity() const
        {
            return size_;
        }

        void reset()
        {
            bytes_stored = 0;
        }

        [[nodiscard]] auto* data() const
        {
            return data_;
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
            size_t remaining_bytes = remainder();
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
