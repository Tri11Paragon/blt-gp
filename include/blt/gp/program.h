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

#include <blt/gp/stack.h>
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
#include <iostream>

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
    
    template<typename Return, typename... Args>
    class operation
    {
        public:
            using function_t = std::function<Return(Args...)>;
            
            operation(const operation& copy) = default;
            
            operation(operation&& move) = default;
            
            explicit operation(const function_t& functor): func(functor)
            {}
            
            explicit operation(function_t&& functor): func(std::move(functor))
            {}
            
            template<blt::u64 index>
            [[nodiscard]] inline constexpr blt::size_t getByteOffset() const
            {
                blt::size_t offset = 0;
                blt::size_t current_index = 0;
                ((offset += (current_index++ > index ? stack_allocator::aligned_size<Args>() : 0)), ...);
                return offset;
            }
            
            template<typename CurrentArgument, blt::u64 index>
            inline CurrentArgument& getArgument(stack_allocator& allocator) const
            {
                auto bytes = getByteOffset<index>();
                return allocator.from<CurrentArgument>(bytes);
            }
            
            template<blt::u64... indices>
            inline Return sequence_to_indices(stack_allocator& allocator, std::integer_sequence<blt::u64, indices...>) const
            {
                return func(getArgument<Args, indices>(allocator)...);
            }
            
            [[nodiscard]] inline Return operator()(stack_allocator& allocator) const
            {
                auto seq = std::make_integer_sequence<blt::u64, sizeof...(Args)>();
                return sequence_to_indices(allocator, seq);
            }
        
        private:
//            template<typename T, blt::size_t index>
//            static inline T& access_pack_index(blt::span<void*> args)
//            {
//                return *reinterpret_cast<T*>(args[index]);
//            }
//
//            template<typename T, T... indexes>
//            Return function_evaluator(blt::span<void*> args, std::integer_sequence<T, indexes...>)
//            {
//                return func(access_pack_index<Args, indexes>(args)...);
//            }
            
            function_t func;
    };
    
    
}

#endif //BLT_GP_PROGRAM_H
