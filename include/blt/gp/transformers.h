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
#include <blt/std/expected.h>

namespace blt::gp
{
    
    class crossover_t
    {
        public:
            enum class error_t
            {
                NO_VALID_TYPE,
                TREE_TOO_SMALL
            };
            struct result_t
            {
                tree_t child1;
                tree_t child2;
            };
            
            /**
             * child1 and child2 are copies of the parents, the result of selecting a crossover point and performing standard subtree crossover.
             * the parents are not modified during this process
             * @param program reference to the global program container responsible for managing these trees
             * @param p1 reference to the first parent
             * @param p2 reference to the second parent
             * @return expected pair of child otherwise returns error enum
             */
            virtual blt::expected<result_t, error_t> apply(gp_program& program, const tree_t& p1, const tree_t& p2); // NOLINT
    };
    
}

#endif //BLT_GP_TRANSFORMERS_H
