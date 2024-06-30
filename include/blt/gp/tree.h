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

#include <utility>
#include <stack>

namespace blt::gp
{
    
    struct op_container_t
    {
        op_container_t(detail::callable_t& func, detail::transfer_t& transfer, bool is_value):
                func(func), transfer(transfer), is_value(is_value)
        {}
        
        detail::callable_t& func;
        detail::transfer_t& transfer;
        bool is_value;
    };
    
    class evaluation_context
    {
            friend class tree_t;
        
        private:
            explicit evaluation_context()
            {}
            
            blt::gp::stack_allocator values;
    };
    
    class tree_t
    {
        public:
            [[nodiscard]] inline std::vector<op_container_t>& get_operations()
            {
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
            
            evaluation_context evaluate(void* context);
            
            /**
             * Helper template for returning the result of the last evaluation
             */
            template<typename T>
            T get_evaluation_value(evaluation_context& context)
            {
                return context.values.pop<T>();
            }
            
            /**
             * Helper template for returning the result of the last evaluation
             */
            template<typename T>
            T& get_evaluation_ref(evaluation_context& context)
            {
                return context.values.from<T>(0);
            }
            
            /**
             * Helper template for returning the result of evaluation (this calls it)
             */
            template<typename T>
            T get_evaluation_value(void* context)
            {
                auto results = evaluate(context);
                return results.values.pop<T>();
            }
        
        private:
            std::vector<op_container_t> operations;
            blt::gp::stack_allocator values;
            blt::size_t depth;
    };
    
    class population_t
    {
        public:
            std::vector<tree_t>& getIndividuals()
            {
                return individuals;
            }
        
        private:
            std::vector<tree_t> individuals;
    };
}

#endif //BLT_GP_TREE_H
