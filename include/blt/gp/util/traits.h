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

}

#endif //BLT_GP_TRAITS_H
