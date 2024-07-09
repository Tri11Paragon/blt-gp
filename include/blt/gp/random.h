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

#ifndef BLT_GP_RANDOM_H
#define BLT_GP_RANDOM_H

#include <blt/std/types.h>
#include <blt/std/random.h>

namespace blt::gp
{
    
    class random_t
    {
        public:
            explicit random_t(blt::size_t seed): seed(seed)
            {}
            
            void set_seed(blt::size_t s)
            {
                seed = s;
            }
        
        
        private:
            blt::size_t seed;
    };
    
}

#endif //BLT_GP_RANDOM_H
