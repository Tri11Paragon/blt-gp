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
#include <utility>
#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <type_traits>

namespace blt::gp
{
    class stack_allocator
    {
            constexpr static blt::size_t PAGE_SIZE = 0x1000;
            constexpr static blt::size_t MAX_ALIGNMENT = 8;
        public:
            /**
             * Pushes an instance of an object on to the stack
             * @tparam T type to push
             * @param value universal reference to the object to push
             */
            template<typename T>
            void push(T&& value)
            {
                using NO_REF_T = std::remove_reference_t<T>;
                auto ptr = allocate_bytes<T>();
                head->metadata.offset = static_cast<blt::u8*>(ptr) + aligned_size<T>();
                new(ptr) NO_REF_T(std::forward<T>(value));
            }
            
            template<typename T>
            T pop()
            {
                constexpr static auto TYPE_SIZE = aligned_size<T>();
                if (head == nullptr)
                    throw std::runtime_error("Silly boi the stack is empty!");
                if (head->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(aligned_size<T>()))
                    throw std::runtime_error((std::string("Mismatched Types! Not enough space left in block! Bytes: ") += std::to_string(
                            head->used_bytes_in_block()) += " Size: " + std::to_string(sizeof(T))).c_str());
                // make copy
                T t = *reinterpret_cast<T*>(head->metadata.offset - TYPE_SIZE);
                // call destructor
                reinterpret_cast<T*>(head->metadata.offset - TYPE_SIZE)->~T();
                head->metadata.offset -= TYPE_SIZE;
                if (head->used_bytes_in_block() == 0)
                {
                    auto ptr = head;
                    head = head->metadata.prev;
                    std::free(ptr);
                }
                return t;
            }
            
            template<typename T>
            T& from(blt::size_t bytes)
            {
                constexpr static auto TYPE_SIZE = aligned_size<T>();
                auto remaining_bytes = static_cast<blt::i64>(bytes);
                blt::i64 bytes_into_block = 0;
                block* blk = head;
                while (remaining_bytes > 0)
                {
                    if (blk == nullptr)
                        throw std::runtime_error("Requested size is beyond the scope of this stack!");
                    auto bytes_available = blk->used_bytes_in_block() - remaining_bytes;
                    bytes_into_block = remaining_bytes;
                    if (bytes_available < 0)
                    {
                        remaining_bytes = -bytes_available;
                        blk = head->metadata.prev;
                    } else
                        break;
                }
                if (blk == nullptr)
                    throw std::runtime_error("Some nonsense is going on. This function already smells");
                if (blk->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(aligned_size<T>()))
                    throw std::runtime_error((std::string("Mismatched Types! Not enough space left in block! Bytes: ") += std::to_string(
                            blk->used_bytes_in_block()) += " Size: " + std::to_string(sizeof(T))).c_str());
                return *reinterpret_cast<T*>((blk->metadata.offset - bytes_into_block) - TYPE_SIZE);
            }
            
            void pop_bytes(blt::ptrdiff_t bytes)
            {
                while (bytes > 0)
                {
                    auto diff = head->used_bytes_in_block() - bytes;
                    // if there is not enough room left to pop completely off the block, then move to the next previous block
                    // and pop from it, update the amount of bytes to reflect the amount removed from the current block
                    if (diff <= 0)
                    {
                        bytes -= head->used_bytes_in_block();
                        head = head->metadata.prev;
                    } else // otherwise update the offset pointer
                        head->metadata.offset -= bytes;
                }
            }
            
            template<typename... Args>
            void call_destructors()
            {
                blt::size_t offset = 0;
                ((from<Args>(offset).~Args(), offset += stack_allocator::aligned_size<Args>()), ...);
            }
            
            [[nodiscard]] bool empty() const
            {
                if (head == nullptr)
                    return true;
                if (head->metadata.prev != nullptr)
                    return false;
                return head->used_bytes_in_block() == 0;
            }
            
            [[nodiscard]] blt::ptrdiff_t bytes_in_head() const
            {
                if (head == nullptr)
                    return 0;
                return head->used_bytes_in_block();
            }
            
            stack_allocator() = default;
            
            stack_allocator(const stack_allocator& copy) = delete;
            
            stack_allocator& operator=(const stack_allocator& copy) = delete;
            
            stack_allocator(stack_allocator&& move) noexcept
            {
                head = move.head;
                move.head = nullptr;
            }
            
            stack_allocator& operator=(stack_allocator&& move) noexcept
            {
                head = move.head;
                move.head = nullptr;
                return *this;
            }
            
            ~stack_allocator()
            {
                block* current = head;
                while (current != nullptr)
                {
                    block* ptr = current;
                    current = current->metadata.prev;
                    std::free(ptr);
                }
            }
            
            template<typename T>
            static inline constexpr blt::size_t aligned_size() noexcept
            {
                return (sizeof(T) + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1);
            }
        
        private:
            struct block
            {
                struct block_metadata_t
                {
                    blt::size_t size = 0;
                    block* next = nullptr;
                    block* prev = nullptr;
                    blt::u8* offset = nullptr;
                } metadata;
                blt::u8 buffer[8]{};
                
                explicit block(blt::size_t size)
                {
                    metadata.size = size;
                    metadata.offset = buffer;
                }
                
                [[nodiscard]] blt::ptrdiff_t storage_size() const
                {
                    return static_cast<blt::ptrdiff_t>(metadata.size - sizeof(typename block::block_metadata_t));
                }
                
                [[nodiscard]] blt::ptrdiff_t used_bytes_in_block() const
                {
                    return static_cast<blt::ptrdiff_t>(metadata.offset - buffer);
                }
                
                [[nodiscard]] blt::ptrdiff_t remaining_bytes_in_block() const
                {
                    return storage_size() - used_bytes_in_block();
                }
            };
            
            template<typename T>
            void* allocate_bytes()
            {
                auto ptr = get_aligned_pointer(sizeof(T));
                if (ptr == nullptr)
                    push_block_for<T>();
                ptr = get_aligned_pointer(sizeof(T));
                if (ptr == nullptr)
                    throw std::bad_alloc();
                return ptr;
            }
            
            void* get_aligned_pointer(blt::size_t bytes)
            {
                if (head == nullptr)
                    return nullptr;
                blt::size_t remaining_bytes = head->remaining_bytes_in_block();
                auto* pointer = static_cast<void*>(head->metadata.offset);
                return std::align(MAX_ALIGNMENT, bytes, pointer, remaining_bytes);
            }
            
            template<typename T>
            void push_block_for()
            {
                push_block(std::max(PAGE_SIZE, to_nearest_page_size(sizeof(T))));
            }
            
            void push_block(blt::size_t size)
            {
                auto blk = allocate_block(size);
                if (head == nullptr)
                {
                    head = blk;
                    return;
                }
                head->metadata.next = blk;
                blk->metadata.prev = head;
                head = blk;
            }
            
            static size_t to_nearest_page_size(blt::size_t bytes)
            {
                constexpr static blt::size_t MASK = ~(PAGE_SIZE - 1);
                return (bytes & MASK) + PAGE_SIZE;
            }
            
            static block* allocate_block(blt::size_t bytes)
            {
                auto size = to_nearest_page_size(bytes);
                auto* data = std::aligned_alloc(PAGE_SIZE, size);
                new(data) block{size};
                return reinterpret_cast<block*>(data);
            }
        
        private:
            block* head = nullptr;
    };
}

#endif //BLT_GP_STACK_H
