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
        op_container_t(const size_t type_size, const operator_id id, const bool is_value):
                type_size(type_size), id(id), is_value(is_value), has_drop(false)
        {}

        op_container_t(const size_t type_size, const operator_id id, const bool is_value, const bool has_drop):
                type_size(type_size), id(id), is_value(is_value), has_drop(has_drop)
        {}
        
        size_t type_size;
        operator_id id;
        bool is_value;
        bool has_drop;
    };
    
    class evaluation_context
    {
        public:
            explicit evaluation_context() = default;
            
            blt::gp::stack_allocator values;
    };
    
    class tree_t
    {
        public:
            explicit tree_t(gp_program& program);
            
            tree_t(const tree_t& copy) = default;
            
            tree_t& operator=(const tree_t& copy)
            {
                if (this == &copy)
                    return *this;
                copy_fast(copy);
                return *this;
            }
            
            /**
             * This function copies the data from the provided tree, will attempt to reserve and copy in one step.
             * will avoid reallocation if enough space is already present.
             */
            void copy_fast(const tree_t& copy)
            {
                if (this == &copy)
                    return;
                values.reserve(copy.values.stored());
                values.reset();
                values.insert(copy.values);

                // operations.reserve(copy.operations.size());
                //
                // auto copy_it = copy.operations.begin();
                // auto op_it = operations.begin();
                //
                // for (; op_it != operations.end(); ++op_it)
                // {
                //     if (op_it->has_drop)
                //     {
                //
                //     }
                //     if (copy_it == copy.operations.end())
                //         break;
                //     *op_it = *copy_it;
                //     if (copy_it->has_drop)
                //     {
                //
                //     }
                //     ++copy_it;
                // }
                // const auto op_it_cpy = op_it;
                // for (;op_it != operations.end(); ++op_it)
                // {
                //     if (op_it->has_drop)
                //     {
                //
                //     }
                // }
                // operations.erase(op_it_cpy, operations.end());
                // for (; copy_it != copy.operations.end(); ++copy_it)
                //     operations.emplace_back(*copy_it);


                operations.clear();
                operations.reserve(copy.operations.size());
                operations.insert(operations.begin(), copy.operations.begin(), copy.operations.end());
            }
            
            tree_t(tree_t&& move) = default;
            
            tree_t& operator=(tree_t&& move) = default;
            
            void clear(gp_program& program);
            
            struct child_t
            {
                blt::ptrdiff_t start;
                // one past the end
                blt::ptrdiff_t end;
            };
            
            [[nodiscard]] inline tracked_vector<op_container_t>& get_operations()
            {
                return operations;
            }
            
            [[nodiscard]] inline const tracked_vector<op_container_t>& get_operations() const
            {
                return operations;
            }
            
            [[nodiscard]] inline stack_allocator& get_values()
            {
                return values;
            }
            
            [[nodiscard]] inline const blt::gp::stack_allocator& get_values() const
            {
                return values;
            }
            
            template<typename T, std::enable_if_t<!(std::is_pointer_v<T> || std::is_null_pointer_v<T>), bool> = true>
            [[nodiscard]] evaluation_context& evaluate(const T& context) const
            {
                return (*func)(*this, const_cast<void*>(static_cast<const void*>(&context)));
            }
            
            [[nodiscard]] evaluation_context& evaluate() const
            {
                return (*func)(*this, nullptr);
            }
            
            blt::size_t get_depth(gp_program& program) const;
            
            /**
             * Helper template for returning the result of the last evaluation
             */
            template<typename T>
            T get_evaluation_value(evaluation_context& context) const
            {
                return context.values.pop<T>();
            }
            
            /**
             * Helper template for returning the result of the last evaluation
             */
            template<typename T>
            T& get_evaluation_ref(evaluation_context& context) const
            {
                return context.values.from<T>(0);
            }
            
            /**
             * Helper template for returning the result of evaluation (this calls it)
             */
            template<typename T, typename Context>
            T get_evaluation_value(const Context& context) const
            {
                return evaluate(context).values.template pop<T>();
            }
            
            template<typename T>
            T get_evaluation_value() const
            {
                return evaluate().values.pop<T>();
            }
            
            void print(gp_program& program, std::ostream& output, bool print_literals = true, bool pretty_indent = false,
                       bool include_types = false) const;
            
            bool check(gp_program& program, void* context) const;
            
            void find_child_extends(gp_program& program, tracked_vector<child_t>& vec, blt::size_t parent_node, blt::size_t argc) const;
            
            // places one past the end of the child. so it's [start, end)
            blt::ptrdiff_t find_endpoint(blt::gp::gp_program& program, blt::ptrdiff_t start) const;
            
            blt::ptrdiff_t find_parent(blt::gp::gp_program& program, blt::ptrdiff_t start) const;
            
            // valid for [begin, end)
            static blt::size_t total_value_bytes(detail::const_op_iter_t begin, detail::const_op_iter_t end)
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
        
        private:
            tracked_vector<op_container_t> operations;
            blt::gp::stack_allocator values;
            detail::eval_func_t* func;
    };
    
    struct fitness_t
    {
        double raw_fitness = 0;
        double standardized_fitness = 0;
        double adjusted_fitness = 0;
        i64 hits = 0;
    };
    
    struct individual_t
    {
        tree_t tree;
        fitness_t fitness;
        
        void copy_fast(const tree_t& copy)
        {
            // fast copy of the tree
            tree.copy_fast(copy);
            // reset fitness
            fitness = {};
        }
        
        individual_t() = delete;
        
        explicit individual_t(tree_t&& tree): tree(std::move(tree))
        {}
        
        explicit individual_t(const tree_t& tree): tree(tree)
        {}
        
        individual_t(const individual_t&) = default;
        
        individual_t(individual_t&&) = default;
        
        individual_t& operator=(const individual_t&) = delete;
        
        individual_t& operator=(individual_t&&) = default;
    };
    
    class population_t
    {
        public:
            class population_tree_iterator
            {
                public:
                    population_tree_iterator(tracked_vector<individual_t>& ind, blt::size_t pos): ind(ind), pos(pos)
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
                    tracked_vector<individual_t>& ind;
                    blt::size_t pos;
            };
            
            tracked_vector<individual_t>& get_individuals()
            {
                return individuals;
            }
            
            [[nodiscard]] const tracked_vector<individual_t>& get_individuals() const
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
        
        private:
            tracked_vector<individual_t> individuals;
    };
}

#endif //BLT_GP_TREE_H
