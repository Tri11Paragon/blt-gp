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
#include <ostream>

namespace blt::gp
{
    
    struct op_container_t
    {
        op_container_t(detail::callable_t& func, detail::transfer_t& transfer, operator_id id, bool is_value):
                func(func), transfer(transfer), id(id), is_value(is_value)
        {}
        
        std::reference_wrapper<detail::callable_t> func;
        std::reference_wrapper<detail::transfer_t> transfer;
        operator_id id;
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
            
            void print(gp_program& program, std::ostream& output, bool print_literals = true, bool pretty_indent = false, bool include_types = false);
        
        private:
            std::vector<op_container_t> operations;
            blt::gp::stack_allocator values;
            blt::size_t depth;
    };
    
    struct individual
    {
        tree_t tree;
        double raw_fitness = 0;
        double standardized_fitness = 0;
        double adjusted_fitness = 0;
    };
    
    struct population_stats
    {
        double overall_fitness = 0;
        double average_fitness = 0;
        double best_fitness = 1;
        double worst_fitness = 0;
        // these will never be null unless your pop is not initialized / fitness eval was not called!
        individual* best_individual = nullptr;
        individual* worst_individual = nullptr;
    };
    
    class population_t
    {
        public:
            class population_tree_iterator
            {
                public:
                    population_tree_iterator(std::vector<individual>& ind, blt::size_t pos): ind(ind), pos(pos)
                    {}
                    
                    auto begin()
                    {
                        return population_tree_iterator(ind, 0);
                    }
                    
                    auto end()
                    {
                        return population_tree_iterator(ind, ind.size());
                    }
                    
                    population_tree_iterator operator++(int)
                    {
                        auto prev = pos++;
                        return {ind, prev};
                    }
                    
                    population_tree_iterator operator++()
                    {
                        return {ind, ++pos};
                    }
                    
                    tree_t& operator*()
                    {
                        return ind[pos].tree;
                    }
                    
                    tree_t& operator->()
                    {
                        return ind[pos].tree;
                    }
                    
                    friend bool operator==(population_tree_iterator a, population_tree_iterator b)
                    {
                        return a.pos == b.pos;
                    }
                    
                    friend bool operator!=(population_tree_iterator a, population_tree_iterator b)
                    {
                        return a.pos != b.pos;
                    }
                
                private:
                    std::vector<individual>& ind;
                    blt::size_t pos;
            };
            
            std::vector<individual>& getIndividuals()
            {
                return individuals;
            }
            
            population_tree_iterator for_each_tree()
            {
                return population_tree_iterator{individuals, 0};
            }
        
        private:
            std::vector<individual> individuals;
    };
}

#endif //BLT_GP_TREE_H
