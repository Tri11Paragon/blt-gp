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
#include <blt/std/assert.h>
#include <blt/std/logging.h>
#include <blt/gp/fwdecl.h>
#include <utility>
#include <stdexcept>
#include <cstdlib>
#include <memory>
#include <type_traits>
#include <cstring>

namespace blt::gp
{
    class stack_allocator
    {
            constexpr static blt::size_t PAGE_SIZE = 0x1000;
            constexpr static blt::size_t MAX_ALIGNMENT = 8;
        public:
            struct size_data_t
            {
                blt::size_t total_size_bytes = 0;
                blt::size_t total_used_bytes = 0;
                blt::size_t total_remaining_bytes = 0;
                blt::size_t total_no_meta_bytes = 0;
                blt::size_t blocks = 0;
            };
            
            
            /**
             * Pushes an instance of an object on to the stack
             * @tparam T type to push
             * @param value universal reference to the object to push
             */
            template<typename T>
            void push(T&& value)
            {
                using NO_REF_T = std::remove_reference_t<T>;
                static_assert(std::is_trivially_copyable_v<NO_REF_T> && "Type must be bitwise copyable!");
                auto ptr = allocate_bytes<T>();
                head->metadata.offset = static_cast<blt::u8*>(ptr) + aligned_size<T>();
                new(ptr) NO_REF_T(std::forward<T>(value));
            }
            
            template<typename T>
            T pop()
            {
                using NO_REF_T = std::remove_reference_t<T>;
                static_assert(std::is_trivially_copyable_v<NO_REF_T> && "Type must be bitwise copyable!");
                constexpr static auto TYPE_SIZE = aligned_size<T>();
                if (head == nullptr)
                    throw std::runtime_error("Silly boi the stack is empty!");
                if (head->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(aligned_size<T>()))
                    throw std::runtime_error((std::string("Mismatched Types! Not enough space left in block! Bytes: ") += std::to_string(
                            head->used_bytes_in_block()) += " Size: " + std::to_string(sizeof(T))).c_str());
                if (head->used_bytes_in_block() == 0)
                    move_back();
                // make copy
                T t = *reinterpret_cast<T*>(head->metadata.offset - TYPE_SIZE);
                // call destructor
                reinterpret_cast<T*>(head->metadata.offset - TYPE_SIZE)->~T();
                // move offset back
                head->metadata.offset -= TYPE_SIZE;
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
#if BLT_DEBUG_LEVEL >= 3
                blt::size_t counter = 0;
#endif
                while (bytes > 0)
                {
#if BLT_DEBUG_LEVEL > 0
                    if (head == nullptr)
                    {
                        BLT_WARN("Head is nullptr, unable to pop bytes!");
                        BLT_WARN("This error is normally caused by an invalid tree!");
#if BLT_DEBUG_LEVEL >= 3
                        BLT_WARN("Made it to %ld iterations", counter);
#endif
                        return;
                    }
#if BLT_DEBUG_LEVEL >= 3
                    counter++;
#endif
#endif
                    auto diff = head->used_bytes_in_block() - bytes;
                    // if there is not enough room left to pop completely off the block, then move to the next previous block
                    // and pop from it, update the amount of bytes to reflect the amount removed from the current block
                    if (diff <= 0)
                    {
                        bytes -= head->used_bytes_in_block();
                        if (diff == 0)
                            break;
                        move_back();
                    } else
                    {
                        // otherwise update the offset pointer
                        head->metadata.offset -= bytes;
                        break;
                    }
                }
            }
            
            /**
             * Warning this function should be used to transfer types, not arrays of types! It will produce an error if you attempt to pass more
             * than one type # of bytes at a time~!
             * @param to stack to push to
             * @param bytes number of bytes to transfer out.
             */
            void transfer_bytes(stack_allocator& to, blt::size_t bytes)
            {
                if (empty())
                    throw std::runtime_error("This stack is empty!");
                if (head->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(bytes))
                    BLT_ABORT("This stack doesn't contain enough data for this type! This is an invalid runtime state!");
                
                if (head->used_bytes_in_block() == 0)
                    move_back();
                
                auto type_size = aligned_size(bytes);
                auto ptr = to.allocate_bytes(bytes);
                to.head->metadata.offset = static_cast<blt::u8*>(ptr) + type_size;
                std::memcpy(ptr, head->metadata.offset - type_size, type_size);
                head->metadata.offset -= type_size;
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
            
            /**
             * Warning this function is slow!
             * @return the size of the stack allocator in bytes
             */
            [[nodiscard]] size_data_t size()
            {
                size_data_t size_data;
                auto* next = head;
                while (next != nullptr)
                {
                    size_data.total_size_bytes += next->metadata.size;
                    size_data.total_no_meta_bytes += next->storage_size();
                    size_data.total_remaining_bytes += next->remaining_bytes_in_block();
                    size_data.total_used_bytes += next->used_bytes_in_block();
                    size_data.blocks++;
                    next = next->metadata.next;
                }
                return size_data;
            }
            
            stack_allocator() = default;
            
            // TODO: cleanup this allocator!
            // if you keep track of type size information you can memcpy between stack allocators as you already only allow trivially copyable types
            stack_allocator(const stack_allocator& copy)
            {
                if (copy.empty())
                    return;
                
                head = nullptr;
                block* list_itr = nullptr;
                
                // start at the beginning of the list
                block* current = copy.head;
                while (current != nullptr)
                {
                    list_itr = current;
                    current = current->metadata.prev;
                }
                // copy all the blocks
                while (list_itr != nullptr)
                {
                    push_block(list_itr->metadata.size);
                    std::memcpy(head->buffer, list_itr->buffer, list_itr->storage_size());
                    head->metadata.size = list_itr->metadata.size;
                    head->metadata.offset = head->buffer + list_itr->used_bytes_in_block();
                    list_itr = list_itr->metadata.next;
                }
            }
            
            stack_allocator& operator=(const stack_allocator& copy) = delete;
            
            stack_allocator(stack_allocator&& move) noexcept
            {
                head = move.head;
                move.head = nullptr;
            }
            
            stack_allocator& operator=(stack_allocator&& move) noexcept
            {
                move.head = std::exchange(head, move.head);
                return *this;
            }
            
            ~stack_allocator()
            {
                free_chain(head);
            }
            
            template<typename T>
            static inline constexpr blt::size_t aligned_size() noexcept
            {
                return aligned_size(sizeof(T));
            }
            
            static inline constexpr blt::size_t aligned_size(blt::size_t size) noexcept
            {
                return (size + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1);
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
#if BLT_DEBUG_LEVEL > 0
                    if (size < PAGE_SIZE)
                    {
                        BLT_WARN("Hey this block is too small, who allocated it?");
                        std::abort();
                    }
#endif
                    metadata.size = size;
                    metadata.offset = buffer;
                }
                
                void reset()
                {
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
                return allocate_bytes(sizeof(T));
            }
            
            void* allocate_bytes(blt::size_t size)
            {
                auto ptr = get_aligned_pointer(size);
                if (ptr == nullptr)
                {
                    if (head != nullptr && head->metadata.next != nullptr)
                    {
                        head = head->metadata.next;
                        if (head != nullptr)
                            head->reset();
                    } else
                        push_block(aligned_size(size));
                }
                ptr = get_aligned_pointer(size);
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
            
            static void free_chain(block* current)
            {
                while (current != nullptr)
                {
                    block* ptr = current;
                    current = current->metadata.prev;
                    std::free(ptr);
                }
            }
            
            inline void move_back()
            {
                auto old = head;
                head = head->metadata.prev;
                if (head == nullptr)
                {
                    head = old;
                    head->reset();
                }
                    //free_chain(old);
                // required to prevent silly memory :3
//                if (head != nullptr)
//                    head->metadata.next = nullptr;
//                std::free(old);
            }
        
        private:
            block* head = nullptr;
    };
}

#endif //BLT_GP_STACK_H
