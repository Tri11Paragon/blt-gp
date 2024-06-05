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

#ifndef BLT_GP_PROGRAM_H
#define BLT_GP_PROGRAM_H

#include <cstddef>
#include <blt/gp/fwdecl.h>
#include <functional>
#include <blt/std/ranges.h>
#include <blt/std/types.h>
#include <blt/std/utility.h>
#include <type_traits>
#include <string_view>
#include <string>
#include <utility>

namespace blt::gp
{
    class identifier
    {
    };
    
    class type
    {
        public:
            template<typename T>
            static type make_type(blt::size_t id)
            {
                return type(sizeof(T), id, blt::type_string<T>());
            }
            
            [[nodiscard]] blt::size_t size() const
            {
                return size_;
            }
            
            [[nodiscard]] blt::size_t id() const
            {
                return id_;
            }
            
            [[nodiscard]] std::string_view name() const
            {
                return name_;
            }
        
        private:
            type(size_t size, size_t id, std::string_view name): size_(size), id_(id), name_(name)
            {}
            
            blt::size_t size_;
            blt::size_t id_;
            std::string name_;
    };
    
    class type_system
    {
        public:
            type_system() = default;
            
            template<typename T>
            inline type register_type()
            {
                types.push_back(type::make_type<T>(types.size()));
                return types.back();
            }
        
        private:
            std::vector<type> types;
    };
    
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
                auto ptr = allocate_bytes<T>();
                head->metadata.offset = ptr + sizeof(T);
                new(ptr) T(std::forward<T>(value));
            }
            
            template<typename T>
            T pop()
            {
                if (head == nullptr)
                    throw std::runtime_error("Silly boi the stack is empty!");
                if (head->remaining_bytes_in_block() - head->storage_size() < sizeof(T))
                    throw std::runtime_error("Mismatched Types!");
                T t = *reinterpret_cast<T*>(head->metadata.offset - sizeof(T));
                head->metadata.offset -= sizeof(T);
                if (head->used_bytes_in_block() == static_cast<blt::ptrdiff_t>(head->storage_size()))
                {
                    auto ptr = head;
                    head = head->metadata.prev;
                    delete ptr;
                }
                return t;
            }
            
            template<typename T>
            T& from(blt::size_t bytes)
            {
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
                return *reinterpret_cast<T*>((blk->metadata.offset - bytes_into_block) - sizeof(T));
            }
            
            [[nodiscard]] bool empty() const
            {
                if (head == nullptr)
                    return true;
                if (head->metadata.prev != nullptr)
                    return false;
                return head->used_bytes_in_block() == static_cast<blt::ptrdiff_t>(head->storage_size());
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
                    delete ptr;
                }
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
                
                [[nodiscard]] blt::ptrdiff_t remaining_bytes_in_block() const
                {
                    return storage_size() - static_cast<blt::ptrdiff_t>(metadata.offset - buffer);
                }
                
                [[nodiscard]] blt::ptrdiff_t used_bytes_in_block() const
                {
                    return static_cast<blt::ptrdiff_t>(metadata.offset - buffer);
                }
            };
            
            template<typename T>
            void* allocate_bytes()
            {
                auto ptr = get_aligned_pointer(sizeof(T), alignof(T));
                if (ptr == nullptr)
                    push_block_for<T>();
                ptr = get_aligned_pointer(sizeof(T), alignof(T));
                if (ptr == nullptr)
                    throw std::bad_alloc();
                return ptr;
            }
            
            void* get_aligned_pointer(blt::size_t bytes, blt::size_t alignment)
            {
                if (head == nullptr)
                    return nullptr;
                blt::size_t remaining_bytes = head->remaining_bytes_in_block();
                auto* pointer = static_cast<void*>(head->metadata.offset);
                return std::align(alignment, bytes, pointer, remaining_bytes);
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
    
    template<typename Return, typename... Args>
    class operation
    {
        public:
            using function_t = std::function<Return(Args...)>;
            
            operation(const operation& copy) = default;
            
            operation(operation&& move) = default;
            
            template<typename T, std::enable_if_t<std::is_same_v<T, function_t>, void>>
            explicit operation(const T& functor): func(functor)
            {}
            
            template<typename T, std::enable_if_t<!std::is_same_v<T, function_t>, void>>
            explicit operation(const T& functor)
            {
                func = [&functor](Args... args) {
                    return functor(args...);
                };
            }
            
            explicit operation(function_t&& functor): func(std::move(functor))
            {}
            
            inline Return operator()(Args... args)
            {
                return func(args...);
            }
            
            Return operator()(blt::span<void*> args)
            {
                auto pack_sequence = std::make_integer_sequence<blt::u64, sizeof...(Args)>();
                return function_evaluator(args, pack_sequence);
            }
            
            std::function<Return(blt::span<void*>)> to_functor()
            {
                return [this](blt::span<void*> args) {
                    return this->operator()(args);
                };
            }
        
        private:
            template<typename T, blt::size_t index>
            static inline T& access_pack_index(blt::span<void*> args)
            {
                return *reinterpret_cast<T*>(args[index]);
            }
            
            template<typename T, T... indexes>
            Return function_evaluator(blt::span<void*> args, std::integer_sequence<T, indexes...>)
            {
                return func(access_pack_index<Args, indexes>(args)...);
            }
            
            function_t func;
    };
    
    
}

#endif //BLT_GP_PROGRAM_H
