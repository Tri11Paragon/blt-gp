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

#ifndef BLT_GP_FUNCTIONAL_H
#define BLT_GP_FUNCTIONAL_H

#include <blt/std/types.h>
#include <blt/gp/stack.h>

namespace blt::gp
{

    namespace detail
    {
        template <typename Type, typename Alloc = std::allocator<Type>>
        struct function_wrapper_t
        {
            using Callback = void (*)(void*, std::vector<Type, Alloc>&, std::vector<Type, Alloc>&);

            template <typename Func>
            explicit function_wrapper_t(Func& func) : context(reinterpret_cast<void*>(&func)), forward_callback(
                                                          [](void* ctx, std::vector<Type, Alloc>& in, std::vector<Type, Alloc>& out)
                                                          {
                                                              using Reflect = util::function_traits<std::decay_t<Func>>;


                                                              Type result = call_func_internal(*static_cast<Func*>(ctx), in,
                                                                  std::make_index_sequence<Reflect::arity>{});
                                                              in.erase(in.end() - Reflect::arity, in.end());

                                                              if constexpr (std::is_trivially_copyable_v<Type>)
                                                              {
                                                                  out.emplace_back(result);
                                                              }
                                                              else if constexpr (std::is_move_constructible_v<Type>)
                                                              {
                                                                  out.emplace_back(std::move(result));
                                                              }
                                                              else
                                                              {
                                                                  out.emplace_back(result);
                                                              }
                                                          })
            {
            }

            void operator()(std::vector<Type, Alloc>& in, std::vector<Type, Alloc>& out) const noexcept
            {
                return forward_callback(context, in, out);
            }

        private:
            template <typename Func, size_t... Is>
            static Type call_func_internal(Func& func, std::vector<Type, Alloc>& in, std::index_sequence<Is...>)
            {
                return func(in[in.size() - sizeof...(Is) + Is]...);
            }

            void* context;
            Callback forward_callback;
        };
    }

    struct function_id_t
    {
        explicit function_id_t(const size_t id) : id(id)
        {
        }

        explicit operator size_t() const
        {
            return id;
        }

        [[nodiscard]] size_t get() const
        {
            return id;
        }

    private:
        size_t id;
    };

    struct function_info_t
    {
        function_id_t id;
        size_t argument_count;
        bool stores_in_stack;

        function_info_t(const function_id_t id, const size_t argument_count, const bool stores_in_stack) :
            id(id), argument_count(argument_count), stores_in_stack(stores_in_stack)
        {
        }

        [[nodiscard]] bool is_terminal() const
        {
            return argument_count == 0;
        }
    };

    template <typename Type, template<typename> typename Alloc = std::allocator>
    struct function_resolver_t
    {
        explicit function_resolver_t() = default;

        template <typename Func>
        function_id_t push(Func& func, bool store_in_ephemeral_stack = false)
        {
            // resolve types
            using traits = util::function_traits<std::decay_t<Func>>;

            // storage
            auto id = functions.size();
            functions.emplace_back(detail::function_wrapper_t<Type, Alloc<Type>>{func});
            function_information.emplace_back(function_info_t{
                function_id_t{id}, traits::arity, store_in_ephemeral_stack
            });
            store_in_stack.emplace_back(store_in_ephemeral_stack);

            return function_id_t{id};
        }

        void forward(const function_id_t operator_id, std::vector<Type, Alloc<Type>>& in, std::vector<Type, Alloc<Type>>& out)
        {
            functions[operator_id.get()](in, out);
        }

        [[nodiscard]] const function_info_t& get(const function_id_t operator_id) const
        {
            return function_information[operator_id.get()];
        }

        [[nodiscard]] bool stores_in_stack(const function_id_t operator_id) const
        {
            return store_in_stack[operator_id.get()];
        }

        [[nodiscard]] std::string to_string(const function_id_t operator_id) const
        {
            std::string data;
            data += '[';
            data += std::to_string(operator_id.get());
            data += " => (";
            for (size_t i = 0; i < function_information[operator_id.get()].argument_count; ++i)
            {
                data += blt::type_string<Type>();
                data += ", ";
            }
            data += ") -> ";
            data += blt::type_string<Type>();
            data += ']';
            return data;
        }

    protected:
        std::vector<u32, Alloc<u32>> store_in_stack;
        std::vector<detail::function_wrapper_t<Type, Alloc<Type>>, Alloc<detail::function_wrapper_t<Type, Alloc<Type>>>> functions;
        std::vector<function_info_t, Alloc<function_info_t>> function_information;
    };
}

#endif //BLT_GP_FUNCTIONAL_H
