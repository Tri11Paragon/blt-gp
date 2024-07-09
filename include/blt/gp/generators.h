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

#ifndef BLT_GP_GENERATORS_H
#define BLT_GP_GENERATORS_H

#include <blt/gp/fwdecl.h>
#include <blt/gp/tree.h>

namespace blt::gp
{
    
    struct generator_arguments
    {
        gp_program& program;
        type_id root_type;
        blt::size_t min_depth;
        blt::size_t max_depth;
    };
    
    struct initializer_arguments
    {
        gp_program& program;
        type_id root_type;
        blt::size_t size;
        blt::size_t min_depth;
        blt::size_t max_depth;
        
        [[nodiscard]] generator_arguments to_gen_args() const
        {
            return {program, root_type, min_depth, max_depth};
        }
    };
    
    // base class for any kind of tree generator
    class tree_generator_t
    {
        public:
            virtual tree_t generate(const generator_arguments& args) = 0;
            
            virtual ~tree_generator_t() = default;
    };
    
    class grow_generator_t : public tree_generator_t
    {
        public:
            tree_t generate(const generator_arguments& args) final;
    };
    
    class full_generator_t : public tree_generator_t
    {
        public:
            tree_t generate(const generator_arguments& args) final;
    };
    
    class population_initializer_t
    {
        public:
            virtual population_t generate(const initializer_arguments& args) = 0;
            
            virtual ~population_initializer_t() = default;
    };
    
    class grow_initializer_t : public population_initializer_t
    {
        public:
            population_t generate(const initializer_arguments& args) final;
        
        private:
            grow_generator_t grow;
    };
    
    class full_initializer_t : public population_initializer_t
    {
        public:
            population_t generate(const initializer_arguments& args) final;
        
        private:
            full_generator_t full;
    };
    
    class half_half_initializer_t : public population_initializer_t
    {
        public:
            population_t generate(const initializer_arguments& args) final;
        
        private:
            grow_generator_t grow;
            full_generator_t full;
    };
    
    class ramped_half_initializer_t : public population_initializer_t
    {
        public:
            population_t generate(const initializer_arguments& args) final;
        
        private:
            grow_generator_t grow;
            full_generator_t full;
    };
    
}

#endif //BLT_GP_GENERATORS_H
