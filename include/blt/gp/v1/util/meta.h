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

#ifndef BLT_GP_UTIL_META_H
#define BLT_GP_UTIL_META_H

#include <type_traits>

namespace blt::gp::detail
{
    template <typename T>
    using remove_cv_ref = std::remove_cv_t<std::remove_reference_t<T>>;


    template <typename...>
    struct first_arg;

    template <typename First, typename... Args>
    struct first_arg<First, Args...>
    {
        using type = First;
    };

    template <>
    struct first_arg<>
    {
        using type = void;
    };

    template <bool b, typename... types>
    struct is_same;

    template <typename... types>
    struct is_same<true, types...> : std::false_type
    {
    };

    template <typename... types>
    struct is_same<false, types...> : std::is_same<types...>
    {
    };

    template <typename... types>
    constexpr bool is_same_v = is_same<sizeof...(types) == 0, types...>::value;

    struct empty_t
    {
    };
}

#endif //BLT_GP_UTIL_META_H
