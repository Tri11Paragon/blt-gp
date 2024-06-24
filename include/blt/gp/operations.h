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
#include <type_traits>

namespace blt::gp
{
    namespace detail
    {
        using callable_t = std::function<void(void*, stack_allocator&)>;
        struct empty_t
        {
        };
    }
    
    template<typename Return, typename... Args>
    struct call_with
    {
        template<blt::u64 index>
        [[nodiscard]] inline constexpr static blt::size_t getByteOffset()
        {
            blt::size_t offset = 0;
            blt::size_t current_index = 0;
            ((offset += (current_index++ > index ? stack_allocator::aligned_size<Args>() : 0)), ...);
            return offset;
        }
        
        template<typename Func, blt::u64... indices, typename... ExtraArgs>
        inline static constexpr Return exec_sequence_to_indices(Func&& func, stack_allocator& allocator, std::integer_sequence<blt::u64, indices...>,
                                                                ExtraArgs&& ... args)
        {
            // expands Args and indices, providing each argument with its index calculating the current argument byte offset
            return std::forward<Func>(func)(std::forward<ExtraArgs>(args)..., allocator.from<Args>(getByteOffset<indices>())...);
        }
        
        template<typename Func, typename... ExtraArgs>
        Return operator()(Func&& func, stack_allocator& allocator, ExtraArgs&& ... args)
        {
            constexpr auto seq = std::make_integer_sequence<blt::u64, sizeof...(Args)>();
            Return ret = exec_sequence_to_indices(std::forward<Func>(func), allocator, seq, std::forward<ExtraArgs>(args)...);
            allocator.call_destructors<Args...>();
            allocator.pop_bytes((stack_allocator::aligned_size<Args>() + ...));
            return ret;
        }
    };
    
    template<typename First, typename... Args>
    struct first_arg
    {
        using type = First;
    };
    
    template<>
    struct first_arg<void>
    {
    };
    
    template<typename Return, typename, typename... Args>
    struct call_without_first : public call_with<Return, Args...>
    {
        using call_with<Return, Args...>::call_with;
    };
    
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
            
            [[nodiscard]] constexpr inline Return operator()(stack_allocator& allocator) const
            {
                if constexpr (sizeof...(Args) == 0)
                {
                    return func();
                } else
                {
                    return call_with<Return, Args...>()(func, allocator);
                }
            }
            
            [[nodiscard]] constexpr inline Return operator()(void* context, stack_allocator& allocator) const
            {
                // should be an impossible state
                if constexpr (sizeof...(Args) == 0)
                {
                    BLT_ABORT("Cannot pass context to function without arguments!");
                }
                auto& ctx_ref = *static_cast<typename first_arg<Args...>::type*>(context);
                if constexpr (sizeof...(Args) == 1)
                {
                    return func(ctx_ref);
                } else
                {
                    return call_without_first<Return, Args...>()(func, allocator, ctx_ref);
                }
            }
            
            template<typename Context>
            [[nodiscard]] detail::callable_t make_callable() const
            {
                return [this](void* context, stack_allocator& values) {
                    if constexpr (sizeof...(Args) == 0)
                    {
                        values.push(this->operator()(values));
                    } else
                    {
                        // annoying hack.
                        if constexpr (std::is_same_v<Context, typename first_arg<Args...>::type>)
                        {
                            // first arg is context
                            values.push(this->operator()(context, values));
                        } else
                        {
                            // first arg isn't context
                            values.push(this->operator()(values));
                        }
                    }
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
    operation_t(Return(*)(Args...)) -> operation_t<Return(Args...)>;

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
