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
#include <blt/std/types.h>

namespace blt::gp
{
    
    struct op_container_t
    {
        blt::gp::operator_id op_id;
        blt::u16 depth;
    };
    
    class tree_t
    {
        public:
            [[nodiscard]] inline std::vector<op_container_t>& get_operations()
            {
                valid = false;
                return operations;
            }
            
            [[nodiscard]] inline const std::vector<op_container_t>& get_operations() const
            {
                return operations;
            }
            
            [[nodiscard]] inline blt::gp::stack_allocator& get_values()
            {
                return values;
            }
            
            void setDepth(blt::size_t d)
            {
                depth = d;
                valid = true;
            }
            
            blt::size_t getDepth()
            {
                if (valid)
                    return depth;
                valid = true;
                return 0;
            }
        
        private:
            bool valid = false;
            std::vector<op_container_t> operations;
            blt::gp::stack_allocator values;
            blt::size_t depth;
    };
    
    class population_t
    {
        public:
        
        private:
            std::vector<tree_t> individuals;
    };
}

#endif //BLT_GP_TREE_H
