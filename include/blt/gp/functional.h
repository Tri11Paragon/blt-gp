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

#include <blt/gp/typing.h>
#include <blt/std/types.h>
#include <blt/gp/stack.h>

namespace blt::gp
{
    template <template<typename...> typename Vec>
    struct function_resolver_t;

    namespace detail
    {
        template <template<typename...> typename Vec>
        struct function_wrapper_t
        {
            friend struct function_resolver_t<Vec>;

            using Callback = void (*)(void*, byte_stack_t<Vec>&, byte_stack_t<Vec>&);

            template <typename Func>
            explicit function_wrapper_t(Func& func) : context(reinterpret_cast<void*>(&func)), forward_callback(
                                                          [](void* ctx, byte_stack_t<Vec>& in, byte_stack_t<Vec>& out)
                                                          {
                                                              using Reflect = util::function_traits<std::decay_t<Func>>;
                                                              static_assert(std::is_trivially_copyable_v<typename Reflect::return_type>,
                                                                            "Function doesn't return trivially copyable type!");

                                                              decltype(auto) result = in.template call_with_arguments<Func
                                                                  &>(*static_cast<Func*>(ctx));
                                                              in.pop_bytes(util::apply_t<offsets_of, typename Reflect::args_tuple>::align_size);

                                                              out.push(result);
                                                          })
            {
            }

            void operator()(byte_stack_t<Vec>& in, byte_stack_t<Vec>& out) const noexcept
            {
                return forward_callback(context, in, out);
            }

        private:
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

    template <template<typename...> typename Vec = std::vector>
    struct function_info_t
    {
        function_id_t id;
        type_info_t return_type;
        Vec<type_info_t> argument_types;
        bool stores_in_stack;

        function_info_t(const function_id_t id, const type_info_t& return_type, Vec<type_info_t> argument_types, const bool stores_in_stack) :
            id(id), return_type(return_type), argument_types(std::move(argument_types)), stores_in_stack(stores_in_stack)
        {
        }

        [[nodiscard]] bool is_terminal() const
        {
            return argument_types.empty();
        }
    };

    template <template<typename...> typename Vec = std::vector>
    struct function_resolver_t
    {
        explicit function_resolver_t(type_resolver_t& resolver) : resolver(&resolver)
        {
        }

        template <typename Func>
        function_id_t push(Func& func, bool store_in_ephemeral_stack = false)
        {
            return push(func, void_function(), store_in_ephemeral_stack);
        }

        template <typename Func, typename AntiFunc>
        function_id_t push(Func& func, AntiFunc& anti_func, bool store_in_ephemeral_stack = false)
        {
            // resolve types
            using traits = util::function_traits<std::decay_t<Func>>;
            using applied_register_t = util::apply_t<register_t, typename traits::args_tuple>;

            applied_register_t arguments{*resolver};
            auto return_type = resolver->register_type<typename traits::return_type>();

            // storage
            auto id = functions.size();
            functions.emplace_back(detail::function_wrapper_t<Vec>{func});
            anti_functions.emplace_back(detail::function_wrapper_t<Vec>{anti_func});
            function_information.emplace_back(function_info_t<Vec>{
                function_id_t{id}, return_type, std::move(arguments.argument_types), store_in_ephemeral_stack
            });
            store_in_stack.emplace_back(store_in_ephemeral_stack);

            return function_id_t{id};
        }

        void forward(const function_id_t operator_id, byte_stack_t<Vec>& in, byte_stack_t<Vec>& out)
        {
            functions[operator_id.get()](in, out);
        }

        void backward(const function_id_t operator_id, byte_stack_t<Vec>& in, byte_stack_t<Vec>& out)
        {
            if (!has_backwards(operator_id))
                return;
            anti_functions[operator_id.get()](in, out);
        }

        const function_info_t<Vec>& get(const function_id_t operator_id) const
        {
            return function_information[operator_id.get()];
        }

        [[nodiscard]] bool has_backwards(const function_id_t operator_id) const
        {
            return anti_functions[operator_id.get()].context != &void_function();
        }

        [[nodiscard]] bool stores_in_stack(const function_id_t operator_id) const
        {
            return store_in_stack[operator_id.get()];
        }

        [[nodiscard]] std::string to_string(const function_id_t operator_id) const
        {
            std::string data;
            auto function_info = function_information[operator_id.get()];
            data += '[';
            data += std::to_string(operator_id.get());
            data += " => (";
            for (const auto& arg : function_info.argument_types)
            {
                data += resolver->get_name(arg.id);
                data += ", ";
            }
            data += ") -> ";
            data += resolver->get_name(function_info.return_type.id);
            data += ']';
            return data;
        }

    protected:
        type_resolver_t* resolver;
        Vec<u32> store_in_stack;
        Vec<detail::function_wrapper_t<Vec>> functions;
        Vec<detail::function_wrapper_t<Vec>> anti_functions;
        Vec<function_info_t<Vec>> function_information;

        static auto& void_function()
        {
            static auto func = []()
            {
                return 0.f;
            };
            return func;
        }

    private:
        template <typename... Args>
        struct register_t
        {
            explicit register_t(type_resolver_t& resolver)
            {
                (argument_types.emplace_back(resolver.register_type<Args>()), ...);
            }

            Vec<type_info_t> argument_types;
        };
    };
}

#endif //BLT_GP_FUNCTIONAL_H
