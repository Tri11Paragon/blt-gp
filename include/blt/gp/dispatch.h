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

#ifndef BLT_GP_DISPATCH_H
#define BLT_GP_DISPATCH_H

#include <blt/std/types.h>
#include <blt/gp/util/traits.h>
#include <utility>
#include <functional>

#include "blt/std/utility.h"

namespace blt::gp
{
    template <typename Dispatch>
    struct dispatch_caller_t
    {
        static size_t constexpr Size = Dispatch::Index + 1;

        explicit dispatch_caller_t(const Dispatch& dispatch) : dispatcher(dispatch)
        {
        }

        explicit dispatch_caller_t(Dispatch&& dispatch) : dispatcher(std::move(dispatch))
        {
        }

        template <typename... Args>
        decltype(auto) dispatch(const size_t index, Args&&... args) const
        {
            if (index > Dispatch::Index)
                throw std::out_of_range("dispatch index out of range: " + std::to_string(index));
            return dispatcher._dispatch(index, std::forward<Args>(args)...);
        }

        template <typename... Args>
        decltype(auto) operator()(const size_t index, Args&&... args) const
        {
            return dispatch(index, std::forward<Args>(args)...);
        }

        Dispatch dispatcher;
    };

    template <typename T>
    struct out_variable_t
    {
        explicit out_variable_t(T& t) : t(&t)
        {
        }

        explicit operator const T&() const
        {
            return t;
        }

        explicit operator T&()
        {
            return t;
        }

        T* t;
    };

    // DO NOT DIRECTLY INTERFACE WITH THESE
    namespace detail
    {
        template <typename F>
        struct callable_holder
        {
            std::decay_t<F> func;

            explicit callable_holder(F&& f) : func(std::forward<F>(f))
            {
            }

            template<typename... Args>
            decltype(auto) operator()(Args&&... args) const
            {
                return std::invoke(func, std::forward<Args>(args)...);
            }
        };

        struct base_t
        {
            template <typename... Args>
            void _dispatch(const size_t, Args&&...) const
            {

            }
        };

        template <size_t I, typename Func, typename Base>
        struct dispatch_t
        {
            friend dispatch_caller_t<dispatch_t>;

            static size_t constexpr Index = I;

            template<typename GFunc>
            explicit dispatch_t(GFunc&& func, Base base) : base(base), func(std::forward<Func>(func))
            {
            }

            template <typename Func2>
            auto add(Func2&& func2) const
            {
                return dispatch_t<I + 1, Func2, dispatch_t>(std::forward<Func2>(func2), *this);
            }

            template <typename Func2>
            auto add(const out_variable_t<size_t> index_out, Func2& func2) const
            {
                *index_out.t = I + 1;
                return dispatch_t<I + 1, Func2, dispatch_t>(func2, *this);
            }

            auto build() const
            {
                return dispatch_caller_t<dispatch_t>{*this};
            }

        public:
            template <typename... Args>
            std::invoke_result_t<Func, Args...> _dispatch(const size_t index, Args&&... args) const
            {
                if (index == I)
                {
                    return func(std::forward<Args>(args)...);
                }
                if constexpr (!std::is_same_v<Base, base_t>)
                    return base._dispatch(index, std::forward<Args>(args)...);
                else
                    BLT_UNREACHABLE;
            }

            Base base;
            callable_holder<Func> func;
        };
    }


    template <typename Func>
    auto make_dispatcher(Func&& base_func)
    {
        return detail::dispatch_t<0, Func, detail::base_t>{std::forward<Func>(base_func), detail::base_t{}};
    }
}

#endif //BLT_GP_DISPATCH_H
