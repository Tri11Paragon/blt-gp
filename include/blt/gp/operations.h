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

#ifndef BLT_GP_OPERATIONS_H
#define BLT_GP_OPERATIONS_H

#include <blt/std/types.h>
#include <blt/gp/typesystem.h>
#include <blt/gp/stack.h>
#include <functional>

namespace blt::gp
{
    template<typename>
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
                if constexpr (sizeof...(Args) == 0)
                {
                    return func();
                } else
                {
                    constexpr auto seq = std::make_integer_sequence<blt::u64, sizeof...(Args)>();
                    Return ret = exec_sequence_to_indices(allocator, seq);
                    allocator.call_destructors<Args...>();
                    allocator.pop_bytes((stack_allocator::aligned_size<Args>() + ...));
                    return ret;
                }
            }
            
            [[nodiscard]] std::function<void(stack_allocator&)> make_callable() const
            {
                return [this](stack_allocator& values) {
                    values.push(this->operator()(values));
                };
            }
            
            [[nodiscard]] inline constexpr blt::size_t get_argc() const
            {
                return sizeof...(Args);
            }
        
        private:
            function_t func;
    };
    
    template<typename Return, typename Class, typename... Args>
    class operation_t<Return (Class::*)(Args...) const> : public operation_t<Return(Args...)>
    {
        public:
            using operation_t<Return(Args...)>::operation_t;
    };
    
    template<typename Lambda>
    operation_t(Lambda) -> operation_t<decltype(&Lambda::operator())>;
    
    template<typename Return, typename... Args>
    operation_t(Return (*)(Args...)) -> operation_t<Return(Args...)>;

//    templat\e<typename Return, typename Class, typename... Args>
//    operation_t<Return(Args...)> make_operator(Return (Class::*)(Args...) const lambda)
//    {
//        // https://ventspace.wordpress.com/2022/04/11/quick-snippet-c-type-trait-templates-for-lambda-details/
//    }
//
//    template<typename Lambda>
//    operation_t<decltype(&Lambda::operator())> make_operator(Lambda&& lambda)
//    {
//        return operation_t<decltype(&Lambda::operator())>(std::forward(lambda));
//    }
//
//    template<typename Return, typename... Args>
//    operation(std::function<Return(Args...)>)  -> operation<Return(Args...)>;
}

#endif //BLT_GP_OPERATIONS_H
