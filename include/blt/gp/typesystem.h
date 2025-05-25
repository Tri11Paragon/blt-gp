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

#ifndef BLT_GP_TYPESYSTEM_H
#define BLT_GP_TYPESYSTEM_H

#include <blt/std/hashmap.h>
#include <blt/std/types.h>
#include <blt/std/ranges.h>
#include <blt/std/utility.h>
#include <blt/std/memory.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/stack.h>
#include <random>

namespace blt::gp
{
    struct operator_id : integer_type<u64>
    {
        using integer_type::integer_type;
    };

    struct type_id : integer_type<u64>
    {
        using integer_type::integer_type;

        // debug helper.
        [[nodiscard]] std::string name(gp_program& program) const;
    };

    class type
    {
    public:
        type() = default;

        template <typename T>
        static type make_type(const type_id id)
        {
            return type(stack_allocator::aligned_size<T>(), id, blt::type_string<T>(), detail::has_func_drop_v<detail::remove_cv_ref<T>>);
        }

        [[nodiscard]] size_t size() const
        {
            return size_;
        }

        [[nodiscard]] type_id id() const
        {
            return id_;
        }

        [[nodiscard]] std::string_view name() const
        {
            return name_;
        }

        [[nodiscard]] bool has_ephemeral_drop() const
        {
            return has_ephemeral_drop_;
        }
    private:
        type(const size_t size, const type_id id, const std::string_view name, const bool has_ephemeral_drop): size_(size), id_(id), name_(name),
            has_ephemeral_drop_(has_ephemeral_drop)
        {
        }

        size_t size_{};
        type_id id_{};
        std::string name_{};
        bool has_ephemeral_drop_ = false;
    };

    /**
     * Is a provider for the set of types possible in a GP program
     * also provides a set of functions for converting between C++ types and BLT GP types
     */
    class type_provider
    {
    public:
        type_provider() = default;

        template <typename T>
        void register_type()
        {
            if (has_type<T>())
                return;
            auto t = type::make_type<T>(types.size());
            types.insert({blt::type_string_raw<T>(), t});
            types_from_id[t.id()] = t;
        }

        template <typename T>
        type get_type()
        {
            return types[blt::type_string_raw<T>()];
        }

        template <typename T>
        bool has_type()
        {
            return types.find(blt::type_string_raw<T>()) != types.end();
        }

        type get_type(type_id id)
        {
            return types_from_id[id];
        }

        /**
         * This function is slow btw
         * @param engine
         * @return
         */
        type select_type(std::mt19937_64& engine)
        {
            std::uniform_int_distribution dist(0ul, types.size() - 1);
            auto offset = dist(engine);
            auto itr = types.begin();
            for ([[maybe_unused]] auto _ : blt::range(0ul, offset))
                itr = itr++;
            return itr->second;
        }

    private:
        hashmap_t<std::string, type> types;
        expanding_buffer<type> types_from_id;
    };
}

template <>
struct std::hash<blt::gp::operator_id>
{
    std::size_t operator()(const blt::gp::operator_id& s) const noexcept
    {
        return std::hash<blt::gp::operator_id::value_type>{}(s);
    }
};

template <>
struct std::hash<blt::gp::type_id>
{
    std::size_t operator()(const blt::gp::type_id& s) const noexcept
    {
        return std::hash<blt::gp::type_id::value_type>{}(s);
    }
};

#endif //BLT_GP_TYPESYSTEM_H
