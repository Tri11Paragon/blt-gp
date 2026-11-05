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

#ifndef BLT_GP_TRAITS_H
#define BLT_GP_TRAITS_H

#include <functional>
#include <tuple>
#include <type_traits>
#include <iostream>
#include <string_view>

namespace blt::gp::util
{

    template<typename T>
    struct function_traits;

    // Plain function type: R(Args...)
    template<typename R, typename... Args>
    struct function_traits<R(Args...)>
    {
        using return_type = R;
        using args_tuple = std::tuple<Args...>;

        static constexpr std::size_t arity = sizeof...(Args);
        static constexpr bool is_noexcept = false;

        template<std::size_t I>
        using arg = std::tuple_element_t<I, args_tuple>;
    };

    // Function pointer: R (*)(Args...)
    template<typename R, typename... Args>
    struct function_traits<R (*)(Args...)> : function_traits<R(Args...)>
    {
    };

    // Function reference: R (&)(Args...)
    template<typename R, typename... Args>
    struct function_traits<R (&)(Args...)> : function_traits<R(Args...)>
    {
    };

    // Member function pointer: R (C::*)(Args...)
    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...)> : function_traits<R(Args...)>
    {
        using class_type = C;
    };

    // Const member function pointer: R (C::*)(Args...) const
    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) const> : function_traits<R(Args...)>
    {
        using class_type = C;
    };

    // Volatile member function pointer
    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) volatile> : function_traits<R(Args...)>
    {
        using class_type = C;
    };

    // Const volatile member function pointer
    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) const volatile> : function_traits<R(Args...)>
    {
        using class_type = C;
    };

    // Functor/lambda fallback
    template<typename F>
    struct function_traits : function_traits<decltype(&F::operator())>
    {
    };

    template<typename R, typename... Args>
    struct function_traits<R(Args...) noexcept> : function_traits<R(Args...)>
    {
        static constexpr bool is_noexcept = true;
    };

    template<typename R, typename... Args>
    struct function_traits<R (*)(Args...) noexcept> : function_traits<R(Args...) noexcept>
    {
    };

    template<typename R, typename... Args>
    struct function_traits<R (&)(Args...) noexcept> : function_traits<R(Args...) noexcept>
    {
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) noexcept> : function_traits<R(Args...) noexcept>
    {
        using class_type = C;
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) const noexcept> : function_traits<R(Args...) noexcept>
    {
        using class_type = C;
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) volatile noexcept> : function_traits<R(Args...) noexcept>
    {
        using class_type = C;
    };

    template<typename C, typename R, typename... Args>
    struct function_traits<R (C::*)(Args...) const volatile noexcept> : function_traits<R(Args...) noexcept>
    {
        using class_type = C;
    };

    template<typename R, typename... Args>
    struct function_traits<std::function<R(Args...)>> : function_traits<R(Args...)>
    {
    };

    template<template<typename...> typename Func, typename T>
    struct apply;

    template<template<typename...> typename Func, template<typename...> typename C, typename... Args>
    struct apply<Func, C<Args...>>
    {
        using type = Func<Args...>;
    };

    template<template<typename...> typename Func, typename T>
    using apply_t = typename apply<Func, T>::type;

    template<typename T>
    struct skip_once;

    template<template<typename...> typename C>
    struct skip_once<C<>>
    {
    };

    template<template<typename...> typename C, typename First, typename... Args>
    struct skip_once<C<First, Args...>>
    {
        using first = First;
        using remainder = C<Args...>;
        using next = skip_once<remainder>;
    };

    template<bool V, typename... Args>
    struct yield_types
    {
        static constexpr bool value = V;
        using value_type = bool;
        using type = yield_types<V, Args...>;
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    template<bool V, typename... Args>
    constexpr static bool yield_types_v = yield_types<V, Args...>::value;

    template<bool V, size_t... Args>
    struct yield_index
    {
        static constexpr bool value = V;
        using value_type = bool;
        using type = yield_index<V, Args...>;
        constexpr operator value_type() const noexcept { return value; }
        constexpr value_type operator()() const noexcept { return value; }
    };

    template<bool V, size_t... Args>
    constexpr static bool yield_index_v = yield_index<V, Args...>::value;

    template<typename... TupleArgs>
    struct assert_equals_t
    {
        template<typename A, typename B, size_t I>
        static constexpr void assert_types()
        {
            static_assert(yield_index_v<std::is_same_v<std::decay_t<A>, std::decay_t<B>>, I>, "Argument Type Mismatch");
        }

        template<typename... Args, size_t... Is>
        static constexpr void with_indexes(std::index_sequence<Is...>)
        {
            (assert_types<Args, TupleArgs, Is>(), ...);
        }

        template<typename... Args>
        static constexpr auto compare()
        {
            with_indexes<Args...>(std::index_sequence_for<Args...>{});
            return true;
        }
    };

    template<typename Tuple, typename... Args>
    constexpr void assert_tuple_contains() {
        apply_t<assert_equals_t, Tuple>{}.template compare<Args...>();
    }


}

#endif //BLT_GP_TRAITS_H
