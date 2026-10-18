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
        using CopyFn = void (*)(void*, void*) noexcept;
        using MoveFn = void (*)(void*, void*) noexcept;
        using DestroyFn = void (*)(void*) noexcept;

        template<typename T>
        CopyFn build_copy_fn()
        {
            return [](void* src, void* dst) noexcept
            {
                new (dst) T(*static_cast<T*>(src));
            };
        }

        template<typename T>
        MoveFn build_move_fn()
        {
            return [](void* src, void* dst) noexcept
            {
                new (dst) T(std::move(*static_cast<T*>(src)));
            };
        }

        template<typename T>
        DestroyFn build_destroy_fn()
        {
            return [](void* src)
            {
                static_cast<T*>(src)->~T();
            };
        }
    }

    struct type_info_t
    {
        detail::CopyFn copy_fn = nullptr;
        detail::MoveFn move_fn = nullptr;
        detail::DestroyFn destroy_fn = nullptr;
        size_t size = 0;

        [[nodiscard]] bool requires_destruction() const noexcept
        {
            return destroy_fn != nullptr;
        }

        template<typename T>
        static type_info_t build()
        {
            type_info_t info;
            info.copy_fn = detail::build_copy_fn<T>();
            info.move_fn = detail::build_move_fn<T>();
            info.destroy_fn = detail::build_destroy_fn<T>();
            info.size = sizeof(T);
            return info;
        }
    };

    struct type_resolver_t
    {
        hashmap_t<std::type_info, size_t> type_indexes;
        std::vector<type_info_t> type_infos;
        size_t next_type_index = 0;

        template<typename T>
        void register_type()
        {
            const auto type_index = type_indexes.find(typeid(T));
            if (type_index == type_indexes.end())
            {
                type_indexes[typeid(T)] = next_type_index;
                type_infos.push_back(type_info_t::build<T>());
                ++next_type_index;
            }
        }

        const type_info_t& operator[](const size_t type_index) const
        {
            return type_infos[type_index];
        }

        template<typename T>
        [[nodiscard]] const type_info_t& at() const
        {
            return type_infos[type_indexes.at(typeid(T))];
        }
    };


    struct tree_t
    {

    };
}

#endif //BLT_GP_TREE_H
