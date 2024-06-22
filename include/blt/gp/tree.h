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

#include <blt/gp/typesystem.h>
#include <blt/gp/stack.h>
#include <blt/gp/fwdecl.h>

namespace blt::gp
{
    class tree_t
    {
        public:
            [[nodiscard]] inline std::vector<blt::gp::operator_id>& get_operations()
            {
                return operations;
            }
            
            [[nodiscard]] inline blt::gp::stack_allocator& get_values()
            {
                return values;
            }
        
        private:
            std::vector<blt::gp::operator_id> operations;
            blt::gp::stack_allocator values;
    };
}

#endif //BLT_GP_TREE_H
