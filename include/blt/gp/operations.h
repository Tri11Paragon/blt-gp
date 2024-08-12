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
#include <optional>

namespace blt::gp
{
    namespace detail
    {
        template<typename T>
        using remove_cv_ref = std::remove_cv_t<std::remove_reference_t<T>>;
        
        
        template<typename...>
        struct first_arg;
        
        template<typename First, typename... Args>
        struct first_arg<First, Args...>
        {
            using type = First;
        };
        
        template<>
        struct first_arg<>
        {
            using type = void;
        };
        
        template<bool b, typename... types>
        struct is_same;
        
        template<typename... types>
        struct is_same<true, types...> : public std::false_type
        {
        };
        
        template<typename... types>
        struct is_same<false, types...> : public std::is_same<types...>
        {
        };
        
        template<typename... types>
        constexpr bool is_same_v = is_same<sizeof...(types) == 0, types...>::value;
        
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
            ((offset += (current_index++ > index ? stack_allocator::aligned_size<detail::remove_cv_ref<Args>>() : 0)), ...);
//            BLT_INFO("offset %ld for argument %ld", offset, index);
            return offset;
        }
        
        template<blt::u64... indices>
        void print_args(std::integer_sequence<blt::u64, indices...>)
        {
            BLT_INFO("Arguments:");
            (BLT_INFO("%ld: %s", indices, blt::type_string<Args>().c_str()), ...);
        }
        
        template<typename Func, blt::u64... indices, typename... ExtraArgs>
        inline static constexpr Return exec_sequence_to_indices(Func&& func, stack_allocator& allocator, std::integer_sequence<blt::u64, indices...>,
                                                                ExtraArgs&& ... args)
        {
            //blt::size_t arg_size = (stack_allocator::aligned_size<detail::remove_cv_ref<Args>>() + ...);
            //BLT_TRACE(arg_size);
            // expands Args and indices, providing each argument with its index calculating the current argument byte offset
            return std::forward<Func>(func)(std::forward<ExtraArgs>(args)...,
                                            allocator.from<detail::remove_cv_ref<Args>>(getByteOffset<indices>())...);
        }
        
        template<typename context = void, typename... NoCtxArgs>
        void call_destructors_without_first(stack_allocator& read_allocator, detail::bitmask_t* mask)
        {
            if constexpr (sizeof...(NoCtxArgs) > 0)
            {
                read_allocator.call_destructors<detail::remove_cv_ref<NoCtxArgs>...>(mask);
            }
        }
        
        template<typename Func, typename... ExtraArgs>
        Return operator()(bool has_context, detail::bitmask_t* mask, Func&& func, stack_allocator& read_allocator, ExtraArgs&& ... args)
        {
            constexpr auto seq = std::make_integer_sequence<blt::u64, sizeof...(Args)>();
#if BLT_DEBUG_LEVEL > 0
            try
            {
#endif
            Return ret = exec_sequence_to_indices(std::forward<Func>(func), read_allocator, seq, std::forward<ExtraArgs>(args)...);
            if (has_context)
                call_destructors_without_first<Args...>(read_allocator, mask);
            else
                read_allocator.call_destructors<detail::remove_cv_ref<Args>...>(mask);
            read_allocator.pop_bytes((stack_allocator::aligned_size<detail::remove_cv_ref<Args>>() + ...));
            return ret;
#if BLT_DEBUG_LEVEL > 0
            } catch (const std::runtime_error& e)
            {
                print_args(seq);
                throw std::runtime_error(e.what());
            }
#endif
        }
    };
    
    template<typename Return, typename, typename... Args>
    struct call_without_first : public call_with<Return, Args...>
    {
        using call_with<Return, Args...>::call_with;
    };
    
    template<typename, typename>
    class operation_t;
    
    template<typename ArgType, typename Return, typename... Args>
    class operation_t<ArgType, Return(Args...)>
    {
        public:
            using function_t = ArgType;
            
            constexpr operation_t(const operation_t& copy) = default;
            
            constexpr operation_t(operation_t&& move) = default;
            
            template<typename Functor>
            constexpr explicit operation_t(const Functor& functor, std::optional<std::string_view> name = {}): func(functor), name(name)
            {}
            
            [[nodiscard]] constexpr inline Return operator()(stack_allocator& read_allocator, detail::bitmask_t* mask) const
            {
                if constexpr (sizeof...(Args) == 0)
                {
                    return func();
                } else
                {
                    return call_with<Return, Args...>()(false, mask, func, read_allocator);
                }
            }
            
            [[nodiscard]] constexpr inline Return operator()(void* context, stack_allocator& read_allocator, detail::bitmask_t* mask) const
            {
                // should be an impossible state
                if constexpr (sizeof...(Args) == 0)
                {
                    BLT_ABORT("Cannot pass context to function without arguments!");
                }
                auto& ctx_ref = *static_cast<detail::remove_cv_ref<typename detail::first_arg<Args...>::type>*>(context);
                if constexpr (sizeof...(Args) == 1)
                {
                    return func(ctx_ref);
                } else
                {
                    return call_without_first<Return, Args...>()(true, mask, func, read_allocator, ctx_ref);
                }
            }
            
            template<typename Context>
            [[nodiscard]] detail::callable_t make_callable() const
            {
                return [this](void* context, stack_allocator& read_allocator, stack_allocator& write_allocator, detail::bitmask_t* mask) {
                    if constexpr (detail::is_same_v<Context, detail::remove_cv_ref<typename detail::first_arg<Args...>::type>>)
                    {
                        // first arg is context
                        write_allocator.push(this->operator()(context, read_allocator, mask));
                    } else
                    {
                        // first arg isn't context
                        write_allocator.push(this->operator()(read_allocator, mask));
                    }
                };
            }
            
            [[nodiscard]] inline constexpr std::optional<std::string_view> get_name() const
            {
                return name;
            }
            
            inline constexpr const auto& get_function() const
            {
                return func;
            }
            
            operator_id id = -1;
        private:
            function_t func;
            std::optional<std::string_view> name;
    };
    
    template<typename ArgType, typename Return, typename Class, typename... Args>
    class operation_t<ArgType, Return (Class::*)(Args...) const> : public operation_t<ArgType, Return(Args...)>
    {
        public:
            using operation_t<ArgType, Return(Args...)>::operation_t;
    };
    
    template<typename Lambda>
    operation_t(Lambda)
    ->
    operation_t<Lambda, decltype(&Lambda::operator())>;
    
    template<typename Return, typename... Args>
    operation_t(Return(*)
            (Args...)) ->
    operation_t<Return(*)(Args...), Return(Args...)>;
    
    template<typename Lambda>
    operation_t(Lambda, std::optional<std::string_view>
               ) ->
    operation_t<Lambda, decltype(&Lambda::operator())>;
    
    template<typename Return, typename... Args>
    operation_t(Return(*)
            (Args...), std::optional<std::string_view>) ->
    operation_t<Return(*)(Args...), Return(Args...)>;
}

#endif //BLT_GP_OPERATIONS_H
