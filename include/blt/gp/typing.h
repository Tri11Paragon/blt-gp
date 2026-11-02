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

#ifndef BLT_GP_TYPING_H
#define BLT_GP_TYPING_H

#include <type_traits>
#include <blt/std/types.h>

namespace blt::gp
{

    namespace detail
    {
        using input_ptr = const void*;
        using output_ptr = void*;
        // src, dst
        using CopyFunction = void (*)(input_ptr, output_ptr);
        using MoveFunction = void (*)(input_ptr, output_ptr);
        using DestroyFunction = void (*)(void*);
    }

    struct type_id_t
    {
        type_id_t() = default;

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
        size_t id = 0;
    };

    struct type_info_t
    {
        type_id_t id = type_id_t{0};
        size_t size = 0;
        size_t aligned_size = 0;

        detail::CopyFunction copy_function = nullptr;
        detail::MoveFunction move_function = nullptr;
        detail::DestroyFunction destroy_function = nullptr;

        template <typename T>
        static type_info_t build(const type_id_t id)
        {
            type_info_t info;
            info.id = id;
            info.size = sizeof(T);
            info.aligned_size = detail::align_bytes(sizeof(T));

            if constexpr (!std::is_trivially_destructible_v<T>)
            {
                info.destroy_function = [](void* ptr)
                {
                    static_cast<T*>(ptr)->~T();
                };
            }

            if constexpr (!std::is_trivially_copyable_v<T> && std::is_copy_constructible_v<T>)
            {
                info.copy_function = [](detail::input_ptr src, detail::output_ptr dst)
                {
                    new (dst) T(*static_cast<const T*>(src));
                };
            }

            if constexpr (!std::is_trivially_copyable_v<T> && std::is_move_constructible_v<T>)
            {
                info.move_function = [](detail::input_ptr src, detail::output_ptr dst)
                {
                    new (dst) T(std::move(*static_cast<T*>(src)));
                };
            }

            return info;
        }
    };

    struct type_resolver_t
    {
        template <typename T>
        type_info_t register_type()
        {
            const auto type_index = type_indexes.find(typeid(T).hash_code());
            if (type_index == type_indexes.end())
            {
                type_indexes[typeid(T).hash_code()] = type_id_t{next_type_index};
                type_infos.push_back(type_info_t::build<T>(type_id_t{next_type_index}));
                type_names.push_back(blt::type_string<T>());
                ++next_type_index;
            }
            return type_infos[type_indexes[typeid(T).hash_code()].get()];
        }

        template <typename T>
        [[nodiscard]] type_id_t get() const
        {
            return type_indexes.at(typeid(T).hash_code());
        }

        template <typename T>
        [[nodiscard]] const type_info_t& get_info() const
        {
            return type_infos.at(get<T>().get());
        }

        [[nodiscard]] const type_info_t& get_info(const type_id_t type_id) const
        {
            return type_infos.at(type_id.get());
        }

        [[nodiscard]] const std::string& get_name(const type_id_t type_id) const
        {
            return type_names.at(type_id.get());
        }

    private:
        hashmap_t<size_t, type_id_t> type_indexes;
        size_t next_type_index = 0;

        std::vector<type_info_t> type_infos;
        std::vector<std::string> type_names;
    };

    struct type_offset_t
    {
        size_t offset = 0;
        type_info_t type_info;

        type_offset_t(const size_t offset, const type_info_t& type_info) : offset(offset), type_info(type_info)
        {
        }
    };
}

#endif //BLT_GP_TYPING_H
