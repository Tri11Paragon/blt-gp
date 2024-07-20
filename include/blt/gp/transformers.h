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
#include <blt/gp/generators.h>
#include <blt/std/expected.h>

namespace blt::gp
{
    
    class crossover_t
    {
        public:
            using op_iter = std::vector<blt::gp::op_container_t>::iterator;
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
            struct crossover_point_t
            {
                blt::ptrdiff_t p1_crossover_point;
                blt::ptrdiff_t p2_crossover_point;
            };
            struct config_t
            {
                // number of times crossover will try to pick a valid point in the tree. this is purely based on the return type of the operators
                blt::u16 max_crossover_tries = 5;
                // if we fail to find a point in the tree, should we search forward from the last point to the end of the operators?
                bool should_crossover_try_forward = false;
                // avoid selecting terminals when doing crossover
                bool avoid_terminals = false;
            };
            
            crossover_t() = default;
            
            explicit crossover_t(const config_t& config): config(config)
            {}
            
            blt::expected<crossover_t::crossover_point_t, error_t> get_crossover_point(gp_program& program, const tree_t& c1, const tree_t& c2) const;
            
            static blt::ptrdiff_t find_endpoint(blt::gp::gp_program& program, const std::vector<blt::gp::op_container_t>& container,
                                                blt::ptrdiff_t start);
            
            static void transfer_forward(blt::gp::stack_allocator& from, blt::gp::stack_allocator& to, op_iter begin, op_iter end);
            static void transfer_backward(blt::gp::stack_allocator& from, blt::gp::stack_allocator& to, op_iter begin, op_iter end);
            
            /**
             * child1 and child2 are copies of the parents, the result of selecting a crossover point and performing standard subtree crossover.
             * the parents are not modified during this process
             * @param program reference to the global program container responsible for managing these trees
             * @param p1 reference to the first parent
             * @param p2 reference to the second parent
             * @return expected pair of child otherwise returns error enum
             */
            virtual blt::expected<result_t, error_t> apply(gp_program& program, const tree_t& p1, const tree_t& p2); // NOLINT
            
            virtual ~crossover_t() = default;
        
        protected:
            config_t config;
    };
    
    class mutation_t
    {
        public:
            struct config_t
            {
                blt::size_t replacement_min_depth = 2;
                blt::size_t replacement_max_depth = 6;
                
                std::reference_wrapper<tree_generator_t> generator;
                
                config_t(tree_generator_t& generator): generator(generator) // NOLINT
                {}
                
                config_t();
            };
            
            mutation_t() = default;
            
            explicit mutation_t(const config_t& config): config(config)
            {}
            
            virtual tree_t apply(gp_program& program, const tree_t& p); // NOLINT
            
            virtual ~mutation_t() = default;
        
        protected:
            config_t config;
    };
    
}

#endif //BLT_GP_TRANSFORMERS_H
