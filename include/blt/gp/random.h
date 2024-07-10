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

#define BLT_RANDOM_FUNCTION blt::random::murmur_random64
#define BLT_RANDOM_FLOAT blt::random::murmur_float64
#define BLT_RANDOM_DOUBLE blt::random::murmur_double64
    
    class random_t
    {
        public:
            explicit random_t(blt::u64 seed): seed(seed)
            {}
            
            void set_seed(blt::u64 s)
            {
                seed = s;
            }
            
            float get_float()
            {
                return BLT_RANDOM_FLOAT(seed);
            }
            
            double get_double()
            {
                return BLT_RANDOM_DOUBLE(seed);
            }
            
            // [min, max)
            double get_double(double min, double max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            // [min, max)
            float get_float(float min, float max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            i32 get_i32(i32 min, i32 max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            u32 get_u32(u32 min, u32 max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            i64 get_i64(i64 min, i64 max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            u64 get_u64(u64 min, u64 max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            blt::size_t get_size_t(blt::size_t min, blt::size_t max)
            {
                return BLT_RANDOM_FUNCTION(seed, min, max);
            }
            
            bool choice()
            {
                return BLT_RANDOM_DOUBLE(seed) < 0.5;
            }
            
            bool choice(double cutoff)
            {
                return BLT_RANDOM_DOUBLE(seed) <= cutoff;
            }
            
            template<typename Container>
            auto& select(Container& container)
            {
                return container[get_u64(0, container.size())];
            }
            
            template<typename Container>
            const auto& select(const Container& container)
            {
                return container[get_u64(0, container.size())];
            }
        
        private:
            blt::u64 seed;
    };
    
}

#endif //BLT_GP_RANDOM_H
