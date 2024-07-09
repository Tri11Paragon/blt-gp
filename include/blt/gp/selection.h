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

#ifndef BLT_GP_SELECTION_H
#define BLT_GP_SELECTION_H

#include <blt/gp/fwdecl.h>
#include <blt/gp/tree.h>
#include <blt/std/assert.h>

namespace blt::gp
{
    
    class selection_t
    {
        public:
            /**
             * @param program gp program to select with, used in randoms
             * @param pop population to select from
             * @param stats the populations statistics
             * @return
             */
            virtual tree_t& select(gp_program& program, population_t& pop, population_stats& stats) = 0;
            
            virtual void pre_process(gp_program&, population_t&, population_stats&)
            {}
            
            virtual ~selection_t() = default;
    };
    
    class select_best_t : public selection_t
    {
        public:
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
    class select_worst_t : public selection_t
    {
        public:
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
    class select_random_t : public selection_t
    {
        public:
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
    class select_tournament_t : public selection_t
    {
        public:
            explicit select_tournament_t(blt::size_t selection_size = 3): selection_size(selection_size)
            {
                if (selection_size < 1)
                    BLT_ABORT("Unable to select with this size. Must select at least 1 individual!");
            }
            
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
        
        private:
            blt::size_t selection_size;
    };
    
    class select_fitness_proportionate_t : public selection_t
    {
        public:
            void pre_process(gp_program& program, population_t& pop, population_stats& stats) final;
            
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
}

#endif //BLT_GP_SELECTION_H
