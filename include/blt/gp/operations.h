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
#include <blt/gp/util/meta.h>
#include <functional>
#include <type_traits>
#include <optional>

namespace blt::gp
{
    template <typename Return, typename... Args>
    struct call_with
    {
        template <u64 index>
        [[nodiscard]] constexpr static size_t getByteOffset()
        {
            size_t offset = 0;
            size_t current_index = 0;
            ((offset += (current_index++ > index ? stack_allocator::aligned_size<detail::remove_cv_ref<Args>>() : 0)), ...);
            (void)current_index;
            return offset;
        }

        template <u64... indices>
        void print_args(std::integer_sequence<u64, indices...>)
        {
            BLT_INFO("Arguments:");
            (BLT_INFO("%ld: %s", indices, blt::type_string<Args>().c_str()), ...);
        }

        template <typename Func, u64... indices, typename... ExtraArgs>
        static constexpr Return exec_sequence_to_indices(Func&& func, stack_allocator& allocator, std::integer_sequence<u64, indices...>,
                                                         ExtraArgs&&... args)
        {
            //blt::size_t arg_size = (stack_allocator::aligned_size<detail::remove_cv_ref<Args>>() + ...);
            //BLT_TRACE(arg_size);
            // expands Args and indices, providing each argument with its index calculating the current argument byte offset
            return std::forward<Func>(func)(std::forward<ExtraArgs>(args)...,
                                            allocator.from<detail::remove_cv_ref<Args>>(getByteOffset<indices>())...);
        }

        template<typename T>
        static void call_drop(stack_allocator& read_allocator, const size_t offset)
        {
            if constexpr (blt::gp::detail::has_func_drop_v<detail::remove_cv_ref<T>>)
            {
                auto [type, ptr] = read_allocator.access_pointer<detail::remove_cv_ref<T>>(offset);
                // type is not ephemeral, so we must drop it.
                if (!ptr.bit(0))
                    type.drop();
            }
        }

        static void call_destructors(stack_allocator& read_allocator)
        {
            if constexpr (sizeof...(Args) > 0)
            {
                size_t offset = (stack_allocator::aligned_size<detail::remove_cv_ref<Args>>() + ...) - stack_allocator::aligned_size<
                    detail::remove_cv_ref<typename meta::arg_helper<Args...>::First>>();
                ((call_drop<Args>(read_allocator, offset), offset -= stack_allocator::aligned_size<detail::remove_cv_ref<Args>>()), ...);
                (void)offset;
            }
        }

        template <typename Func, typename... ExtraArgs>
        Return operator()(const bool, Func&& func, stack_allocator& read_allocator, ExtraArgs&&... args)
        {
            constexpr auto seq = std::make_integer_sequence<u64, sizeof...(Args)>();
#if BLT_DEBUG_LEVEL > 0
            try
            {
#endif
            Return ret = exec_sequence_to_indices(std::forward<Func>(func), read_allocator, seq, std::forward<ExtraArgs>(args)...);
            call_destructors(read_allocator);
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

    template <typename Return, typename, typename... Args>
    struct call_without_first : public call_with<Return, Args...>
    {
        using call_with<Return, Args...>::call_with;
    };

    template <typename, typename>
    class operation_t;

    template <typename RawFunction, typename Return, typename... Args>
    class operation_t<RawFunction, Return(Args...)>
    {
    public:
        using function_t = RawFunction;
        using First_Arg = typename blt::meta::arg_helper<Args...>::First;

        constexpr operation_t(const operation_t& copy) = default;

        constexpr operation_t(operation_t&& move) = default;

        template <typename Functor>
        constexpr explicit operation_t(const Functor& functor, const std::optional<std::string_view> name = {}): func(functor), name(name)
        {
        }

        [[nodiscard]] constexpr Return operator()(stack_allocator& read_allocator) const
        {
            if constexpr (sizeof...(Args) == 0)
            {
                return func();
            }
            else
            {
                return call_with<Return, Args...>()(false, func, read_allocator);
            }
        }

        [[nodiscard]] constexpr Return operator()(void* context, stack_allocator& read_allocator) const
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
            }
            else
            {
                return call_without_first<Return, Args...>()(true, func, read_allocator, ctx_ref);
            }
        }

        template <typename Context>
        [[nodiscard]] detail::operator_func_t make_callable() const
        {
            return [this](void* context, stack_allocator& read_allocator, stack_allocator& write_allocator)
            {
                if constexpr (detail::is_same_v<Context, detail::remove_cv_ref<typename detail::first_arg<Args...>::type>>)
                {
                    // first arg is context
                    write_allocator.push(this->operator()(context, read_allocator));
                }
                else
                {
                    // first arg isn't context
                    write_allocator.push(this->operator()(read_allocator));
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

        auto set_ephemeral()
        {
            is_ephemeral_ = true;
            return *this;
        }

        [[nodiscard]] bool is_ephemeral() const
        {
            return is_ephemeral_;
        }

        [[nodiscard]] bool return_has_ephemeral_drop() const
        {
            return detail::has_func_drop_v<detail::remove_cv_ref<Return>>;
        }

        operator_id id = -1;

    private:
        function_t func;
        std::optional<std::string_view> name;
        bool is_ephemeral_ = false;
    };

    template <typename RawFunction, typename Return, typename Class, typename... Args>
    class operation_t<RawFunction, Return (Class::*)(Args...) const> : public operation_t<RawFunction, Return(Args...)>
    {
    public:
        using operation_t<RawFunction, Return(Args...)>::operation_t;
    };

    template <typename Lambda>
    operation_t(Lambda) -> operation_t<Lambda, decltype(&Lambda::operator())>;

    template <typename Return, typename... Args>
    operation_t(Return (*)(Args...)) -> operation_t<Return(*)(Args...), Return(Args...)>;

    template <typename Lambda>
    operation_t(Lambda, std::optional<std::string_view>) -> operation_t<Lambda, decltype(&Lambda::operator())>;

    template <typename Return, typename... Args>
    operation_t(Return (*)(Args...), std::optional<std::string_view>) -> operation_t<Return(*)(Args...), Return(Args...)>;
}

#endif //BLT_GP_OPERATIONS_H
