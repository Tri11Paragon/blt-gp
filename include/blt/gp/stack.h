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
#include <blt/std/meta.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/stats.h>
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
            constexpr static blt::size_t MAX_ALIGNMENT = 8;
            template<typename T>
            using NO_REF_T = std::remove_cv_t<std::remove_reference_t<T>>;
            using Allocator = aligned_allocator;
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
                           << (data.total_size_bytes != 0 ? (static_cast<double>(data.total_used_bytes) / static_cast<double>(data.total_size_bytes) *
                                                             100) : 0) << "%); space left: " << data.total_remaining_bytes << "]";
                    return stream;
                }
            };
            
            template<typename T>
            static inline constexpr blt::size_t aligned_size() noexcept
            {
                return aligned_size(sizeof(NO_REF_T<T>));
            }
            
            static inline constexpr blt::size_t aligned_size(blt::size_t size) noexcept
            {
                return (size + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1);
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
            {}
            
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
            
            void copy_from(blt::u8* data, blt::size_t bytes)
            {
                if (bytes == 0 || data == nullptr)
                    return;
                if (bytes + bytes_stored > size_)
                    expand(bytes + size_);
                std::memcpy(data_ + bytes_stored, data, bytes);
                bytes_stored += bytes;
            }
            
            void copy_to(blt::u8* data, blt::size_t bytes)
            {
                if (bytes == 0 || data == nullptr)
                    return;
                std::memcpy(data, data_ + (bytes_stored - bytes), bytes);
            }
            
            template<typename T, typename NO_REF = NO_REF_T<T>>
            void push(const T& t)
            {
                static_assert(std::is_trivially_copyable_v<NO_REF> && "Type must be bitwise copyable!");
                static_assert(alignof(NO_REF) <= MAX_ALIGNMENT && "Type alignment must not be greater than the max alignment!");
                auto ptr = allocate_bytes_for_size(sizeof(NO_REF));
                std::memcpy(ptr, &t, sizeof(NO_REF));
            }
            
            template<typename T, typename NO_REF = NO_REF_T<T>>
            T pop()
            {
                static_assert(std::is_trivially_copyable_v<NO_REF> && "Type must be bitwise copyable!");
                static_assert(alignof(NO_REF) <= MAX_ALIGNMENT && "Type alignment must not be greater than the max alignment!");
                constexpr auto size = aligned_size(sizeof(NO_REF));
#if BLT_DEBUG_LEVEL > 0
                if (bytes_stored < size)
                    BLT_ABORT("Not enough bytes left to pop!");
#endif
                bytes_stored -= size;
                return *reinterpret_cast<T*>(data_ + bytes_stored);
            }
            
            [[nodiscard]] blt::u8* from(blt::size_t bytes) const
            {
#if BLT_DEBUG_LEVEL > 0
                if (bytes_stored < bytes)
                    BLT_ABORT(("Not enough bytes in stack to reference " + std::to_string(bytes) + " bytes requested but " + std::to_string(bytes) +
                               " bytes stored!").c_str());
#endif
                return data_ + (bytes_stored - bytes);
            }
            
            template<typename T, typename NO_REF = NO_REF_T<T>>
            T& from(blt::size_t bytes)
            {
                static_assert(std::is_trivially_copyable_v<NO_REF> && "Type must be bitwise copyable!");
                static_assert(alignof(NO_REF) <= MAX_ALIGNMENT && "Type alignment must not be greater than the max alignment!");
                return *reinterpret_cast<NO_REF*>(from(aligned_size(sizeof(NO_REF)) + bytes));
            }
            
            void pop_bytes(blt::size_t bytes)
            {
#if BLT_DEBUG_LEVEL > 0
                if (bytes_stored < bytes)
                    BLT_ABORT(("Not enough bytes in stack to pop " + std::to_string(bytes) + " bytes requested but " + std::to_string(bytes) +
                               " bytes stored!").c_str());
#endif
                bytes_stored -= bytes;
            }
            
            void transfer_bytes(stack_allocator& to, blt::size_t bytes)
            {
#if BLT_DEBUG_LEVEL > 0
                if (bytes_stored < bytes)
                    BLT_ABORT(("Not enough bytes in stack to transfer " + std::to_string(bytes) + " bytes requested but " + std::to_string(bytes) +
                               " bytes stored!").c_str());
#endif
                auto alg = aligned_size(bytes);
                to.copy_from(*this, alg);
                pop_bytes(alg);
            }
            
            template<typename... Args>
            void call_destructors()
            {
                if constexpr (sizeof...(Args) > 0)
                {
                    blt::size_t offset = (stack_allocator::aligned_size(sizeof(NO_REF_T<Args>)) + ...) -
                                         stack_allocator::aligned_size(sizeof(NO_REF_T<typename blt::meta::arg_helper<Args...>::First>));
                    ((call_drop<Args>(offset), offset -= stack_allocator::aligned_size(sizeof(NO_REF_T<Args>))), ...);
                }
            }
            
            [[nodiscard]] bool empty() const noexcept
            {
                return bytes_stored == 0;
            }
            
            [[nodiscard]] blt::ptrdiff_t remaining_bytes_in_block() const noexcept
            {
                return static_cast<blt::ptrdiff_t>(size_ - bytes_stored);
            }
            
            [[nodiscard]] blt::ptrdiff_t bytes_in_head() const noexcept
            {
                return static_cast<blt::ptrdiff_t>(bytes_stored);
            }
            
            [[nodiscard]] size_data_t size() const noexcept
            {
                size_data_t data;
                
                data.total_used_bytes = bytes_stored;
                data.total_size_bytes = size_;
                data.total_remaining_bytes = remaining_bytes_in_block();
                
                return data;
            }
            
            void reserve(blt::size_t bytes)
            {
                if (bytes > size_)
                    expand_raw(bytes);
            }
            
            [[nodiscard]] blt::size_t stored() const
            {
                return bytes_stored;
            }
            
            [[nodiscard]] blt::size_t internal_storage_size() const
            {
                return size_;
            }
            
            void reset()
            {
                bytes_stored = 0;
            }
        
        private:
            void expand(blt::size_t bytes)
            {
                //bytes = to_nearest_page_size(bytes);
                expand_raw(bytes);
            }
            
            void expand_raw(blt::size_t bytes)
            {
                auto new_data = static_cast<blt::u8*>(get_allocator().allocate(bytes));
                if (bytes_stored > 0)
                    std::memcpy(new_data, data_, bytes_stored);
                get_allocator().deallocate(data_, size_);
                data_ = new_data;
                size_ = bytes;
            }
            
            static size_t to_nearest_page_size(blt::size_t bytes) noexcept
            {
                constexpr static blt::size_t MASK = ~(PAGE_SIZE - 1);
                return (bytes & MASK) + PAGE_SIZE;
            }
            
            void* get_aligned_pointer(blt::size_t bytes) noexcept
            {
                if (data_ == nullptr)
                    return nullptr;
                blt::size_t remaining_bytes = remaining_bytes_in_block();
                auto* pointer = static_cast<void*>(data_ + bytes_stored);
                return std::align(MAX_ALIGNMENT, bytes, pointer, remaining_bytes);
            }
            
            void* allocate_bytes_for_size(blt::size_t bytes)
            {
                auto used_bytes = aligned_size(bytes);
                auto aligned_ptr = get_aligned_pointer(used_bytes);
                if (aligned_ptr == nullptr)
                {
                    expand(size_ + used_bytes);
                    aligned_ptr = get_aligned_pointer(used_bytes);
                }
                if (aligned_ptr == nullptr)
                    throw std::bad_alloc();
                bytes_stored += used_bytes;
                return aligned_ptr;
            }
            
            template<typename T>
            inline void call_drop(blt::size_t offset)
            {
                if constexpr (detail::has_func_drop_v<T>)
                {
                    from<NO_REF_T<T >>(offset).drop();
                }
            }
            
            blt::u8* data_ = nullptr;
            // place in the data_ array which has a free spot.
            blt::size_t bytes_stored = 0;
            blt::size_t size_ = 0;
    };
    
    
}

#endif //BLT_GP_STACK_H
