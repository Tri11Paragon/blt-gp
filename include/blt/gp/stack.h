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
#include <blt/std/allocator.h>
#include <blt/std/meta.h>
#include <blt/gp/fwdecl.h>
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
            constexpr static blt::size_t PAGE_SIZE = 0x1000;
            constexpr static blt::size_t MAX_ALIGNMENT = 8;
            template<typename T>
            using NO_REF_T = std::remove_cv_t<std::remove_reference_t<T>>;
        public:
            struct size_data_t
            {
                blt::size_t total_size_bytes = 0;
                blt::size_t total_used_bytes = 0;
                blt::size_t total_remaining_bytes = 0;
                blt::size_t total_no_meta_bytes = 0;
                
                blt::size_t total_dealloc = 0;
                blt::size_t total_dealloc_used = 0;
                blt::size_t total_dealloc_remaining = 0;
                blt::size_t total_dealloc_no_meta = 0;
                
                blt::size_t blocks = 0;
                
                friend std::ostream& operator<<(std::ostream& stream, const size_data_t& data)
                {
                    stream << "[";
                    stream << data.total_used_bytes << "/";
                    stream << data.total_size_bytes << "(";
                    stream << (static_cast<double>(data.total_used_bytes) / static_cast<double>(data.total_size_bytes) * 100) << "%), ";
                    stream << data.total_used_bytes << "/";
                    stream << data.total_no_meta_bytes << "(";
                    stream << (static_cast<double>(data.total_used_bytes) / static_cast<double>(data.total_no_meta_bytes) * 100)
                           << "%), (empty space: ";
                    stream << data.total_remaining_bytes << ") blocks: " << data.blocks << " || unallocated space: ";
                    stream << data.total_dealloc_used << "/";
                    stream << data.total_dealloc;
                    if (static_cast<double>(data.total_dealloc) > 0)
                        stream << "(" << (static_cast<double>(data.total_dealloc_used) / static_cast<double>(data.total_dealloc) * 100) << "%)";
                    stream << ", ";
                    stream << data.total_dealloc_used << "/";
                    stream << data.total_dealloc_no_meta;
                    if (data.total_dealloc_no_meta > 0)
                        stream << "(" << (static_cast<double>(data.total_dealloc_used) / static_cast<double>(data.total_dealloc_no_meta * 100))
                               << "%)";
                    stream << ", (empty space: " << data.total_dealloc_remaining << ")]";
                    return stream;
                }
            };
            
            void insert(stack_allocator stack)
            {
                if (stack.empty())
                    return;
                // take a copy of the pointer to this stack's blocks
                auto old_head = stack.head;
                // stack is now empty, we have the last reference to it.
                stack.head = nullptr;
                // we don't have any nodes to search through or re-point, we can just assign the head
                if (head == nullptr)
                {
                    head = old_head;
                    return;
                }
                
                // find the beginning of the stack
                auto begin = old_head;
                while (begin->metadata.prev != nullptr)
                    begin = begin->metadata.prev;
                
                // move along blocks with free space, attempt to insert bytes from one stack to another
                auto insert = head;
                while (insert->metadata.next != nullptr && begin != nullptr)
                {
                    if (begin->used_bytes_in_block() <= insert->remaining_bytes_in_block())
                    {
                        std::memcpy(insert->metadata.offset, begin->buffer, begin->used_bytes_in_block());
                        insert->metadata.offset += begin->used_bytes_in_block();
                        auto old_begin = begin;
                        begin = begin->metadata.next;
                        free_block(old_begin);
                    }
                    head = insert;
                    insert = insert->metadata.next;
                }
                if (begin == nullptr)
                    return;
                while (insert->metadata.next != nullptr)
                    insert = insert->metadata.next;
                // if here is space left we can move the pointers around
                insert->metadata.next = begin;
                begin->metadata.prev = insert;
                // find where the head is now and set the head to this new point.
                auto new_head = begin;
                while (new_head->metadata.next != nullptr)
                    new_head = new_head->metadata.next;
                head = new_head;
            }
            
            /**
             * Bytes must be the number of bytes to move, all types must have alignment accounted for
             */
            void copy_from(const stack_allocator& stack, blt::size_t bytes)
            {
                if (bytes == 0)
                    return;
                if (stack.empty())
                {
                    BLT_WARN("This stack is empty, we will copy no bytes from it!");
                    return;
                }
                auto [start_block, bytes_left, start_point] = get_start_from_bytes(stack, bytes);
                
                if (bytes_left > 0)
                {
                    allocate_block_to_head_for_size(bytes_left);
                    std::memcpy(head->metadata.offset, start_point, bytes_left);
                    head->metadata.offset += bytes_left;
                    start_block = start_block->metadata.next;
                }
                // we now copy whole blocks at a time.
                while (start_block != nullptr)
                {
                    allocate_block_to_head_for_size(start_block->used_bytes_in_block());
                    std::memcpy(head->metadata.offset, start_block->buffer, start_block->used_bytes_in_block());
                    head->metadata.offset += start_block->used_bytes_in_block();
                    start_block = start_block->metadata.next;
                }
            }
            
            void copy_from(blt::u8* data, blt::size_t bytes)
            {
                if (bytes == 0 || data == nullptr)
                    return;
                allocate_block_to_head_for_size(bytes);
                std::memcpy(head->metadata.offset, data, bytes);
                head->metadata.offset += bytes;
            }
            
            void copy_to(blt::u8* data, blt::size_t bytes) const
            {
                if (bytes == 0 || data == nullptr)
                    return;
                auto [start_block, bytes_left, start_point] = get_start_from_bytes(*this, bytes);
                
                blt::size_t write_point = 0;
                if (bytes_left > 0)
                {
                    std::memcpy(data + write_point, start_point, bytes_left);
                    write_point += bytes_left;
                    start_block = start_block->metadata.next;
                }
                // we now copy whole blocks at a time.
                while (start_block != nullptr)
                {
                    std::memcpy(data + write_point, start_block->buffer, start_block->used_bytes_in_block());
                    write_point += start_block->used_bytes_in_block();
                    start_block = start_block->metadata.next;
                }
            }
            
            /**
             * Pushes an instance of an object on to the stack
             * @tparam T type to push
             * @param value universal reference to the object to push
             */
            template<typename T>
            void push(const T& value)
            {
                using NO_REF_T = std::remove_cv_t<std::remove_reference_t<T>>;
                static_assert(std::is_trivially_copyable_v<NO_REF_T> && "Type must be bitwise copyable!");
                static_assert(alignof(NO_REF_T) <= MAX_ALIGNMENT && "Type must not be greater than the max alignment!");
                auto ptr = allocate_bytes<NO_REF_T>();
                head->metadata.offset = static_cast<blt::u8*>(ptr) + aligned_size<NO_REF_T>();
                //new(ptr) NO_REF_T(std::forward<T>(value));
                std::memcpy(ptr, &value, sizeof(NO_REF_T));
            }
            
            template<typename T>
            T pop()
            {
                using NO_REF_T = std::remove_cv_t<std::remove_reference_t<T>>;
                static_assert(std::is_trivially_copyable_v<NO_REF_T> && "Type must be bitwise copyable!");
                constexpr static auto TYPE_SIZE = aligned_size<NO_REF_T>();
                
                while (head->used_bytes_in_block() == 0 && move_back());
                if (empty())
                    throw std::runtime_error("Silly boi the stack is empty!");
                
                if (head->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(aligned_size<NO_REF_T>()))
                    throw std::runtime_error((std::string("Mismatched Types! Not enough space left in block! Bytes: ") += std::to_string(
                            head->used_bytes_in_block()) += " Size: " + std::to_string(sizeof(NO_REF_T))).c_str());
                // make copy
                NO_REF_T t = *reinterpret_cast<NO_REF_T*>(head->metadata.offset - TYPE_SIZE);
                // call destructor
                if constexpr (detail::has_func_drop_v<T>)
                    call_drop<NO_REF_T>(0, 0, nullptr);
                // move offset back
                head->metadata.offset -= TYPE_SIZE;
                // moving back allows us to allocate with other data, if there is room.
                while (head->used_bytes_in_block() == 0 && move_back());
                return t;
            }
            
            template<typename T>
            T& from(blt::size_t bytes)
            {
                using NO_REF_T = std::remove_cv_t<std::remove_reference_t<T>>;
                
                constexpr static auto TYPE_SIZE = aligned_size<NO_REF_T>();
                
                auto remaining_bytes = static_cast<blt::ptrdiff_t>(bytes + TYPE_SIZE);
                
                block* blk = head;
                while (remaining_bytes > 0)
                {
                    if (blk == nullptr)
                    {
                        BLT_WARN_STREAM << "Stack state: " << size() << "\n";
                        BLT_WARN_STREAM << "Requested " << bytes << " bytes which becomes " << (bytes + TYPE_SIZE) << "\n";
                        throw std::runtime_error("Requested size is beyond the scope of this stack!");
                    }
                    
                    auto bytes_available = blk->used_bytes_in_block() - remaining_bytes;
                    
                    if (bytes_available < 0)
                    {
                        remaining_bytes -= blk->used_bytes_in_block();
                        blk = blk->metadata.prev;
                    } else
                        break;
                }
                if (blk == nullptr)
                    throw std::runtime_error("Some nonsense is going on. This function already smells");
                if (blk->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(TYPE_SIZE))
                {
                    BLT_WARN_STREAM << size() << "\n";
                    BLT_WARN_STREAM << "Requested " << bytes << " bytes which becomes " << (bytes + TYPE_SIZE) << "\n";
                    BLT_WARN_STREAM << "Block size: " << blk->storage_size() << "\n";
                    BLT_ABORT((std::string("Mismatched Types! Not enough space left in block! Bytes: ") += std::to_string(
                            blk->used_bytes_in_block()) += " Size: " + std::to_string(sizeof(NO_REF_T))).c_str());
                }
                return *reinterpret_cast<NO_REF_T*>(blk->metadata.offset - remaining_bytes);
            }
            
            void pop_bytes(blt::ptrdiff_t bytes)
            {
                if (bytes == 0)
                    return;
                if (empty())
                {
                    BLT_WARN("Cannot pop %ld bytes", bytes);
                    BLT_ABORT("Stack is empty, we cannot pop!");
                }
                while (bytes > 0)
                {
                    if (head == nullptr)
                    {
                        BLT_WARN("The head is null, this stack doesn't contain enough data inside to pop %ld bytes!", bytes);
                        BLT_WARN_STREAM << "Stack State: " << size() << "\n";
                        BLT_ABORT("Stack doesn't contain enough data to preform a pop!");
                    }
                    auto diff = head->used_bytes_in_block() - bytes;
                    // if there is not enough room left to pop completely off the block, then move to the next previous block
                    // and pop from it, update the amount of bytes to reflect the amount removed from the current block
                    if (diff < 0)
                    {
                        bytes -= head->used_bytes_in_block();
                        // reset this head's buffer.
                        head->metadata.offset = head->buffer;
                        move_back();
                    } else
                    {
                        // otherwise update the offset pointer
                        head->metadata.offset -= bytes;
                        break;
                    }
                }
                while (head != nullptr && head->used_bytes_in_block() == 0 && move_back());
            }
            
            /**
             * Warning this function should be used to transfer types, not arrays of types! It will produce an error if you attempt to pass more
             * than one type # of bytes at a time!
             * @param to stack to push to
             * @param bytes number of bytes to transfer out.
             */
            void transfer_bytes(stack_allocator& to, blt::size_t bytes)
            {
                while (head->used_bytes_in_block() == 0 && move_back());
                if (empty())
                    throw std::runtime_error("This stack is empty!");
                
                auto type_size = aligned_size(bytes);
                if (head->used_bytes_in_block() < static_cast<blt::ptrdiff_t>(type_size))
                {
                    BLT_ERROR_STREAM << "Stack State:\n" << size() << "\n" << "Bytes in head: " << bytes_in_head() << "\n";
                    BLT_ABORT(("This stack doesn't contain enough data for this type! " + std::to_string(head->used_bytes_in_block()) + " / " +
                               std::to_string(bytes) + " This is an invalid runtime state!").c_str());
                }
                
                auto ptr = to.allocate_bytes(type_size);
                to.head->metadata.offset = static_cast<blt::u8*>(ptr) + type_size;
                std::memcpy(ptr, head->metadata.offset - type_size, type_size);
                head->metadata.offset -= type_size;
                while (head->used_bytes_in_block() == 0 && move_back());
            }
            
            template<typename... Args>
            void call_destructors(detail::bitmask_t* mask)
            {
                if constexpr (sizeof...(Args) > 0) {
                    blt::size_t offset = (stack_allocator::aligned_size<NO_REF_T<Args>>() + ...) -
                                         stack_allocator::aligned_size<NO_REF_T<typename blt::meta::arg_helper<Args...>::First>>();
                    blt::size_t index = 0;
                    if (mask != nullptr)
                        index = mask->size() - sizeof...(Args);
                    ((call_drop<Args>(offset, index, mask), offset -= stack_allocator::aligned_size<NO_REF_T<Args>>(), ++index), ...);
                    if (mask != nullptr)
                    {
                        auto& mask_r = *mask;
                        for (blt::size_t i = 0; i < sizeof...(Args); i++)
                            mask_r.pop_back();
                    }
                }
            }
            
            [[nodiscard]] bool empty() const noexcept
            {
                if (head == nullptr)
                    return true;
                if (head->metadata.prev != nullptr)
                    return false;
                return head->used_bytes_in_block() == 0;
            }
            
            [[nodiscard]] blt::ptrdiff_t bytes_in_head() const noexcept
            {
                if (head == nullptr)
                    return 0;
                return head->used_bytes_in_block();
            }
            
            /**
             * Warning this function is slow!
             * @return the size of the stack allocator in bytes
             */
            [[nodiscard]] size_data_t size() const noexcept
            {
                size_data_t size_data;
                auto* prev = head;
                while (prev != nullptr)
                {
                    size_data.total_size_bytes += prev->metadata.size;
                    size_data.total_no_meta_bytes += prev->storage_size();
                    size_data.total_remaining_bytes += prev->remaining_bytes_in_block();
                    size_data.total_used_bytes += prev->used_bytes_in_block();
                    size_data.blocks++;
                    prev = prev->metadata.prev;
                }
                if (head != nullptr)
                {
                    auto next = head->metadata.next;
                    while (next != nullptr)
                    {
                        size_data.total_dealloc += next->metadata.size;
                        size_data.total_dealloc_no_meta += next->storage_size();
                        size_data.total_dealloc_remaining += next->remaining_bytes_in_block();
                        size_data.total_dealloc_used += next->used_bytes_in_block();
                        size_data.blocks++;
                        next = next->metadata.next;
                    }
                }
                return size_data;
            }
            
            stack_allocator() = default;
            
            // TODO: cleanup this allocator!
            // if you keep track of type size information you can memcpy between stack allocators as you already only allow trivially copyable types
            stack_allocator(const stack_allocator& copy) noexcept
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
            
            ~stack_allocator() noexcept
            {
                if (head != nullptr)
                {
                    auto blk = head->metadata.next;
                    while (blk != nullptr)
                    {
                        auto ptr = blk;
                        blk = blk->metadata.next;
                        free_block(ptr);
                    }
                }
                free_chain(head);
            }
            
            template<typename T>
            static inline constexpr blt::size_t aligned_size() noexcept
            {
                return aligned_size(sizeof(NO_REF_T<T>));
            }
            
            static inline constexpr blt::size_t aligned_size(blt::size_t size) noexcept
            {
                return (size + (MAX_ALIGNMENT - 1)) & ~(MAX_ALIGNMENT - 1);
            }
            
            inline static constexpr auto metadata_size() noexcept
            {
                return sizeof(typename block::block_metadata_t);
            }
            
            inline static constexpr auto block_size() noexcept
            {
                return sizeof(block);
            }
            
            inline static constexpr auto page_size() noexcept
            {
                return PAGE_SIZE;
            }
            
            inline static constexpr auto page_size_no_meta() noexcept
            {
                return page_size() - metadata_size();
            }
            
            inline static constexpr auto page_size_no_block() noexcept
            {
                return page_size() - block_size();
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
                
                explicit block(blt::size_t size) noexcept
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
                
                void reset() noexcept
                {
                    metadata.offset = buffer;
                }
                
                [[nodiscard]] blt::ptrdiff_t storage_size() const noexcept
                {
                    return static_cast<blt::ptrdiff_t>(metadata.size - sizeof(typename block::block_metadata_t));
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
            
            struct copy_start_point
            {
                block* start_block;
                blt::ptrdiff_t bytes_left;
                blt::u8* start_point;
            };
            
            template<typename T>
            inline void call_drop(blt::size_t offset, blt::size_t index, detail::bitmask_t* mask)
            {
                if constexpr (detail::has_func_drop_v<T>)
                {
                    if (mask != nullptr)
                    {
                        auto& mask_r = *mask;
                        if (!mask_r[index])
                            return;
                    }
                    from<NO_REF_T<T>>(offset).drop();
                }
            }
            
            template<typename T>
            void* allocate_bytes()
            {
                return allocate_bytes(sizeof(NO_REF_T<T>));
            }
            
            void* allocate_bytes(blt::size_t size)
            {
                auto ptr = get_aligned_pointer(size);
                if (ptr == nullptr)
                    allocate_block_to_head_for_size(aligned_size(size));
                ptr = get_aligned_pointer(size);
                if (ptr == nullptr)
                    throw std::bad_alloc();
                return ptr;
            }
            
            /**
             * Moves forward through the list of "deallocated" blocks, if none meet size requirements it'll allocate a new block.
             * This function will take into account the size of the block metadata, but requires the size input to be aligned.
             * It will perform no modification to the size value.
             *
             * The block which allows for size is now at head.
             */
            void allocate_block_to_head_for_size(const blt::size_t size) noexcept
            {
                while (head != nullptr && head->metadata.next != nullptr)
                {
                    head = head->metadata.next;
                    if (head != nullptr)
                        head->reset();
                    if (head->remaining_bytes_in_block() >= static_cast<blt::ptrdiff_t>(size))
                        break;
                }
                if (head == nullptr || head->remaining_bytes_in_block() < static_cast<blt::ptrdiff_t>(size))
                    push_block(size + sizeof(typename block::block_metadata_t));
            }
            
            void* get_aligned_pointer(blt::size_t bytes) noexcept
            {
                if (head == nullptr)
                    return nullptr;
                blt::size_t remaining_bytes = head->remaining_bytes_in_block();
                auto* pointer = static_cast<void*>(head->metadata.offset);
                return std::align(MAX_ALIGNMENT, bytes, pointer, remaining_bytes);
            }
            
            void push_block(blt::size_t size) noexcept
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
            
            static size_t to_nearest_page_size(blt::size_t bytes) noexcept
            {
                constexpr static blt::size_t MASK = ~(PAGE_SIZE - 1);
                return (bytes & MASK) + PAGE_SIZE;
            }
            
            static block* allocate_block(blt::size_t bytes) noexcept
            {
                auto size = to_nearest_page_size(bytes);
                auto* data = std::aligned_alloc(PAGE_SIZE, size);
                //auto* data = get_allocator().allocate(size);
                new(data) block{size};
                return reinterpret_cast<block*>(data);
            }
            
            static void free_chain(block* current) noexcept
            {
                while (current != nullptr)
                {
                    block* ptr = current;
                    current = current->metadata.prev;
                    free_block(ptr);
                    //get_allocator().deallocate(ptr);
                }
            }
            
            static void free_block(block* ptr) noexcept
            {
                std::free(ptr);
            }
            
            inline bool move_back() noexcept
            {
                auto old = head;
                head = head->metadata.prev;
                if (head == nullptr)
                {
                    head = old;
                    return false;
                }
                return true;
            }
            
            [[nodiscard]] inline static copy_start_point get_start_from_bytes(const stack_allocator& stack, blt::size_t bytes)
            {
                auto start_block = stack.head;
                auto bytes_left = static_cast<blt::ptrdiff_t>(bytes);
                blt::u8* start_point = nullptr;
                while (bytes_left > 0)
                {
                    if (start_block == nullptr)
                    {
                        BLT_WARN("This stack doesn't contain enough space to copy %ld bytes!", bytes);
                        BLT_WARN_STREAM << "State: " << stack.size() << "\n";
                        BLT_ABORT("Stack doesn't contain enough data for this copy operation!");
                    }
                    if (start_block->used_bytes_in_block() < bytes_left)
                    {
                        bytes_left -= start_block->used_bytes_in_block();
                        start_block = start_block->metadata.prev;
                    } else if (start_block->used_bytes_in_block() == bytes_left)
                    {
                        start_point = start_block->buffer;
                        break;
                    } else
                    {
                        start_point = start_block->metadata.offset - bytes_left;
                        break;
                    }
                }
                return copy_start_point{start_block, bytes_left, start_point};
            }
        
        private:
            block* head = nullptr;
    };
}

#endif //BLT_GP_STACK_H
