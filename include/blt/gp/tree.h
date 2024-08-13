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
#include <atomic>

namespace blt::gp
{
    
    struct op_container_t
    {
        op_container_t(detail::callable_t& func, blt::size_t type_size, operator_id id, bool is_value):
                func(func), type_size(type_size), id(id), is_value(is_value)
        {}
        
        std::reference_wrapper<detail::callable_t> func;
        blt::size_t type_size;
        //std::reference_wrapper<detail::transfer_t> transfer;
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
            using iter_type = std::vector<op_container_t>::const_iterator;
        public:
            tree_t(): reference_counter(new std::atomic_int64_t(1))
            {}
            
            tree_t(const tree_t& copy): operations(copy.operations), values(copy.values), reference_counter(copy.reference_counter)
            {
                reference_counter->operator++();
            }
            
            tree_t(tree_t&& move) noexcept:
                    operations(std::move(move.operations)), values(std::move(move.values)), reference_counter(move.reference_counter)
            {
                move.reference_counter = nullptr;
            }
            
            tree_t& operator=(const tree_t& copy) = delete;
            
            tree_t& operator=(tree_t&& move) noexcept
            {
                operations = std::exchange(operations, std::move(move.operations));
                values = std::exchange(values, std::move(move.values));
                reference_counter = std::exchange(reference_counter, move.reference_counter);
                return *this;
            }
            
            
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
            
            [[nodiscard]] inline const blt::gp::stack_allocator& get_values() const
            {
                return values;
            }
            
            evaluation_context evaluate(void* context) const;
            
            blt::size_t get_depth(gp_program& program);
            
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
            
            void print(gp_program& program, std::ostream& output, bool print_literals = true, bool pretty_indent = false,
                       bool include_types = false) const;
            
            bool check(gp_program& program, void* context) const;
            
            blt::ptrdiff_t find_endpoint(blt::gp::gp_program& program, blt::ptrdiff_t start);
            
            // valid for [begin, end)
            static blt::size_t total_value_bytes(iter_type begin, iter_type end)
            {
                blt::size_t total = 0;
                for (auto it = begin; it != end; it++)
                {
                    if (it->is_value)
                        total += stack_allocator::aligned_size(it->type_size);
                }
                return total;
            }
            
            [[nodiscard]] blt::size_t total_value_bytes(blt::size_t begin, blt::size_t end) const
            {
                return total_value_bytes(operations.begin() + static_cast<blt::ptrdiff_t>(begin),
                                         operations.begin() + static_cast<blt::ptrdiff_t>(end));
            }
            
            [[nodiscard]] blt::size_t total_value_bytes(blt::size_t begin) const
            {
                return total_value_bytes(operations.begin() + static_cast<blt::ptrdiff_t>(begin), operations.end());
            }
            
            [[nodiscard]] blt::size_t total_value_bytes() const
            {
                return total_value_bytes(operations.begin(), operations.end());
            }
            
            void drop(gp_program& program);
            
            ~tree_t()
            {
                if (reference_counter == nullptr)
                    return;
                reference_counter->operator--();
                if (*reference_counter == 0)
                    delete reference_counter;
            }
        
        private:
            std::vector<op_container_t> operations;
            blt::gp::stack_allocator values;
            std::atomic_int64_t* reference_counter;
    };
    
    struct fitness_t
    {
        double raw_fitness = 0;
        double standardized_fitness = 0;
        double adjusted_fitness = 0;
        blt::i64 hits = 0;
    };
    
    struct individual
    {
        tree_t tree;
        fitness_t fitness;
        
        individual() = default;
        
        explicit individual(tree_t&& tree): tree(std::move(tree))
        {}
        
        explicit individual(const tree_t& tree): tree(tree)
        {}
        
        individual(const individual&) = default;
        
        individual(individual&&) = default;
        
        individual& operator=(const individual&) = delete;
        
        individual& operator=(individual&&) = default;
    };
    
    struct population_stats
    {
        std::atomic<double> overall_fitness = 0;
        std::atomic<double> average_fitness = 0;
        std::atomic<double> best_fitness = 0;
        std::atomic<double> worst_fitness = 1;
        std::vector<double> normalized_fitness{};
        
        void clear()
        {
            overall_fitness = 0;
            average_fitness = 0;
            best_fitness = 0;
            worst_fitness = 0;
            normalized_fitness.clear();
        }
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
            
            std::vector<individual>& get_individuals()
            {
                return individuals;
            }
            
            population_tree_iterator for_each_tree()
            {
                return population_tree_iterator{individuals, 0};
            }
            
            auto begin()
            {
                return individuals.begin();
            }
            
            auto end()
            {
                return individuals.end();
            }
            
            [[nodiscard]] auto begin() const
            {
                return individuals.begin();
            }
            
            [[nodiscard]] auto end() const
            {
                return individuals.end();
            }
            
            void clear()
            {
                individuals.clear();
            }
            
            population_t() = default;
            
            population_t(const population_t&) = default;
            
            population_t(population_t&&) = default;
            
            population_t& operator=(const population_t&) = delete;
            
            population_t& operator=(population_t&&) = default;
        
            void drop(gp_program& program);
        private:
            std::vector<individual> individuals;
    };
}

#endif //BLT_GP_TREE_H
