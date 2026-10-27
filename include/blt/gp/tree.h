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

#ifndef BLT_GP_TREE_H
#define BLT_GP_TREE_H

#include <algorithm>
#include "stack.h"
#include <blt/std/hashmap.h>

namespace blt::gp
{
    namespace detail
    {
        template <template<typename...> typename Vec>
        struct function_wrapper_t
        {
            using Callback = void (*)(void*, byte_stack_t<Vec>&, byte_stack_t<Vec>&);

            template <typename Func>
            explicit function_wrapper_t(Func& func) : context(&func), forward_callback(
                                                          [](void* ctx, byte_stack_t<Vec>& in, byte_stack_t<Vec>& out)
                                                          {
                                                              using Reflect = util::function_traits<std::decay_t<Func>>;
                                                              static_assert(std::is_trivially_copyable_v<typename Reflect::return_type>,
                                                                            "Function doesn't return trivially copyable type!");

                                                              decltype(auto) result = in.template call_with_arguments<Func>(*static_cast<Func*>(ctx));
                                                              in.pop_bytes(util::apply_t<offsets_of, typename Reflect::args_tuple>::align_size);

                                                              out.push(result);
                                                          })
            {
            }

            void operator()(byte_stack_t<Vec>& in, byte_stack_t<Vec>& out) const noexcept
            {
                return forward(in, out);
            }

        private:
            void* context;
            Callback forward_callback;
        };
    }

    struct type_id_t
    {
        explicit type_id_t(const size_t id) : id(id)
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

    struct type_info_t
    {
        type_id_t id = type_id_t{0};
        size_t size = 0;
        size_t aligned_size = 0;

        template <typename T>
        static type_info_t build(const type_id_t id)
        {
            type_info_t info;
            info.id = id;
            info.size = sizeof(T);
            info.aligned_size = detail::align_bytes(sizeof(T));
            return info;
        }
    };

    struct type_resolver_t
    {
        hashmap_t<std::type_info, type_id_t> type_indexes;
        std::vector<type_info_t> type_infos;
        size_t next_type_index = 0;

        template <typename T>
        type_info_t register_type()
        {
            const auto type_index = type_indexes.find(typeid(T));
            if (type_index == type_indexes.end())
            {
                type_indexes[typeid(T)] = type_id_t{next_type_index};
                type_infos.push_back(type_info_t::build<T>(type_id_t{next_type_index}));
                ++next_type_index;
            }
            return type_infos[type_indexes[typeid(T)].get()];
        }

        template <typename T>
        [[nodiscard]] type_id_t get() const
        {
            return type_indexes.at(typeid(T));
        }

        template <typename T>
        [[nodiscard]] const type_info_t& get_info() const
        {
            return type_infos.at(get<T>());
        }

        [[nodiscard]] const type_info_t& get_info(const type_id_t type_id) const
        {
            return type_infos.at(type_id.get());
        }
    };

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
        type_info_t return_type;
        Vec<type_info_t> argument_types;

        function_info_t(const type_info_t& return_type, Vec<type_info_t> argument_types) : return_type(return_type),
                                                                                    argument_types(std::move(argument_types))
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
        function_id_t push(Func& func)
        {
            return push(func, void_function);
        }

        template <typename Func, typename AntiFunc>
        function_id_t push(Func& func, AntiFunc& anti_func)
        {
            // resolve types
            using traits = util::function_traits<std::decay_t<Func>>;
            using applied_register_t = util::apply_t<register_t, typename traits::args>;

            applied_register_t arguments{*resolver};
            auto return_type = resolver->register_type<typename traits::return_type>();
            function_info_t<Vec> func_info{std::move(arguments.argument_types), return_type};

            // storage
            auto id = functions.size();
            functions.emplace_back(func);
            anti_functions.emplace_back(anti_func);
            function_information.emplace_back(std::move(func_info));

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
            return anti_functions[operator_id.get()].context != &void_function;
        }

    protected:
        type_resolver_t* resolver;
        Vec<detail::function_wrapper_t<Vec>> functions;
        Vec<detail::function_wrapper_t<Vec>> anti_functions;
        Vec<function_info_t<Vec>> function_information;

        static auto void_function()
        {
            return detail::function_wrapper_t<Vec>([](byte_stack_t<Vec>&, byte_stack_t<Vec>&)
            {
            });
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

    template <template<typename...> typename Vec = std::vector>
    struct tree_t
    {
        explicit tree_t(function_resolver_t<Vec>& resolver) : resolver(&resolver)
        {
        }



    protected:
        function_resolver_t<Vec>* resolver;
    };
}

#endif //BLT_GP_TREE_H
