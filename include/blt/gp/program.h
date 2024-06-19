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
#include <blt/std/hashmap.h>
#include <blt/std/types.h>
#include <blt/std/utility.h>
#include <type_traits>
#include <string_view>
#include <string>
#include <utility>
#include <iostream>

namespace blt::gp
{
    
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
                types[blt::type_string_raw<T>()](type::make_type<T>(types.size()));
                return types[blt::type_string_raw<T>()];
            }
            
            template<typename T>
            inline type get_type()
            {
                return types[blt::type_string_raw<T>()];
            }
        
        private:
            blt::hashmap_t<std::string, type> types;
    };
    
    template<typename Signature>
    class operation_t;
    
    template<typename Return, typename... Args>
    class operation_t<Return(Args...)>
    {
        public:
            using function_t = std::function<Return(Args...)>;
            
            constexpr operation_t(const operation_t& copy) = default;
            
            constexpr operation_t(operation_t&& move) = default;
            
            template<typename Functor>
            constexpr explicit operation_t(const Functor& functor): func(functor)
            {}
            
            template<blt::u64 index>
            [[nodiscard]] inline constexpr static blt::size_t getByteOffset()
            {
                blt::size_t offset = 0;
                blt::size_t current_index = 0;
                ((offset += (current_index++ > index ? stack_allocator::aligned_size<Args>() : 0)), ...);
                return offset;
            }
            
            template<blt::u64... indices>
            inline constexpr Return exec_sequence_to_indices(stack_allocator& allocator, std::integer_sequence<blt::u64, indices...>) const
            {
                // expands Args and indices, providing each argument with its index calculating the current argument byte offset
                return func(allocator.from<Args>(getByteOffset<indices>())...);
            }
            
            [[nodiscard]] constexpr inline Return operator()(stack_allocator& allocator) const
            {
                constexpr auto seq = std::make_integer_sequence<blt::u64, sizeof...(Args)>();
                Return ret = exec_sequence_to_indices(allocator, seq);
                allocator.call_destructors<Args...>();
                allocator.pop_bytes((stack_allocator::aligned_size<Args>() + ...));
                return ret;
            }
            
            [[nodiscard]] std::function<void(stack_allocator&)> make_callable() const
            {
                return [this](stack_allocator& values) {
                    values.push(this->operator()(values));
                };
            }
            
            [[nodiscard]] blt::size_t get_argc() const
            {
                return sizeof...(Args);
            }
        
        private:
            function_t func;
    };
    
    template<typename Return, typename... Args>
    operation_t(Return (*)(Args...)) -> operation_t<Return(Args...)>;
//
//    template<typename Sig>
//    operation(std::function<Sig>) -> operation<Sig>;
//
//    template<typename Return, typename... Args>
//    operation(std::function<Return(Args...)>)  -> operation<Return(Args...)>;
    
    class gp_program
    {
        public:
            explicit gp_program(type_system system): system(std::move(system))
            {}
            
            template<typename Return, typename... Args>
            void add_operator(const operation_t<Return(Args...)>& op)
            {
                if (op.get_argc() == 0)
                    terminals.push_back(operators.size());
                else
                    non_terminals.push_back(operators.size());
                
                operators.push_back(op.make_callable());
                
                std::vector<type> types;
                (types.push_back(system.get_type<Args>()), ...);
                arg_types.push_back(std::move(types));
                
                return_types.push_back(system.get_type<Return>());
            }
        
        private:
            type_system system;
            std::vector<std::function<void(stack_allocator&)>> operators;
            std::vector<blt::size_t> terminals;
            std::vector<blt::size_t> non_terminals;
            std::vector<type> return_types;
            std::vector<std::vector<type>> arg_types;
    };
    
}

#endif //BLT_GP_PROGRAM_H
