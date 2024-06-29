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

#ifndef BLT_GP_TRANSFORMERS_H
#define BLT_GP_TRANSFORMERS_H

#include <blt/std/utility.h>
#include <blt/gp/fwdecl.h>
#include <blt/gp/tree.h>

namespace blt::gp
{
    
    class crossover_t
    {
        public:
            BLT_ATTRIB_CONST virtual std::pair<tree_t, tree_t> apply(gp_program& program, const tree_t& p1, const tree_t& p2);
            virtual void apply_in_place(gp_program& program, tree_t& p1, tree_t& p2);
    };
    
}

#endif //BLT_GP_TRANSFORMERS_H
