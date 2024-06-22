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
#include <blt/gp/fwdecl.h>

namespace blt::gp
{
    struct operator_id : integer_type<blt::size_t>
    {
        using integer_type<blt::size_t>::integer_type;
    };
    
    struct type_id : integer_type<blt::size_t>
    {
        using integer_type<blt::size_t>::integer_type;
    };
    
    class type
    {
        public:
            type() = default;
            
            template<typename T>
            static type make_type(type_id id)
            {
                return type(sizeof(T), id, blt::type_string<T>());
            }
            
            [[nodiscard]] inline blt::size_t size() const
            {
                return size_;
            }
            
            [[nodiscard]] inline type_id id() const
            {
                return id_;
            }
            
            [[nodiscard]] inline std::string_view name() const
            {
                return name_;
            }
        
        private:
            type(size_t size, type_id id, std::string_view name): size_(size), id_(id), name_(name)
            {}
            
            blt::size_t size_{};
            type_id id_{};
            std::string name_{};
    };
    
    class type_system
    {
        public:
            type_system() = default;
            
            template<typename T>
            inline type register_type()
            {
                types.insert({blt::type_string_raw<T>(), type::make_type<T>(types.size())});
                return types[blt::type_string_raw<T>()];
            }
            
            template<typename T>
            inline type get_type()
            {
                return types[blt::type_string_raw<T>()];
            }
        
        private:
            blt::hashmap_t<std::string, type> types;
    };
}

#endif //BLT_GP_TYPESYSTEM_H
