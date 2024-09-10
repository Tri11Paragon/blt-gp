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
#include <blt/meta/config_generator.h>

namespace blt::gp
{
    namespace detail
    {
        template<typename T>
        inline static constexpr double sum(const T& array)
        {
            double init = 0.0;
            for (double i : array)
                init += i;
            return init;
        }
        
        template<blt::size_t size, typename... Args>
        static constexpr std::array<double, size> aggregate_array(Args... list)
        {
            std::array<double, size> data{list...};
            auto total_prob = sum(data);
            double sum_of_prob = 0;
            for (auto& d : data)
            {
                auto prob = d / total_prob;
                d = prob + sum_of_prob;
                sum_of_prob += prob;
            }
            return data;
        }
    }
    
    class crossover_t
    {
        public:
            struct point_info_t
            {
                type_id return_type;
                operator_info_t* type_operator_info;
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
                blt::f32 traverse_chance = 0.75;
                
                // legacy settings:
                
                // if we fail to find a point in the tree, should we search forward from the last point to the end of the operators?
                bool should_crossover_try_forward = false;
                // avoid selecting terminals when doing crossover
                bool avoid_terminals = false;
            };
            
            crossover_t() = default;
            
            explicit crossover_t(const config_t& config): config(config)
            {}
            
            std::optional<crossover_t::crossover_point_t> get_crossover_point(gp_program& program, const tree_t& c1, const tree_t& c2) const;
            
            std::optional<crossover_t::crossover_point_t> get_crossover_point_traverse(gp_program& program, const tree_t& c1, const tree_t& c2) const;
            
            std::optional<point_info_t> get_point_traverse(gp_program& program, const tree_t& t, std::optional<type_id> type) const;
            
            static std::optional<point_info_t> random_place_of_type(gp_program& program, const tree_t& t, type_id type);
            
            /**
             * child1 and child2 are copies of the parents, the result of selecting a crossover point and performing standard subtree crossover.
             * the parents are not modified during this process
             * @param program reference to the global program container responsible for managing these trees
             * @param p1 reference to the first parent
             * @param p2 reference to the second parent
             * @return expected pair of child otherwise returns error enum
             */
            virtual bool apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2); // NOLINT
            
            virtual ~crossover_t() = default;
        
        protected:
            std::optional<point_info_t> get_point_traverse_retry(gp_program& program, const tree_t& t, std::optional<type_id> type) const;
            
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
            
            virtual bool apply(gp_program& program, const tree_t& p, tree_t& c);
            
            // returns the point after the mutation
            blt::size_t mutate_point(gp_program& program, tree_t& c, blt::size_t node);
            
            virtual ~mutation_t() = default;
        
        protected:
            config_t config;
    };
    
    class advanced_mutation_t : public mutation_t
    {
        public:
            enum class mutation_operator : blt::i32
            {
                EXPRESSION,     // Generate a new random expression
                ADJUST,         // adjust the value of the type. (if it is a function it will mutate it to a different one)
                SUB_FUNC,       // subexpression becomes argument to new random function. Other args are generated.
                JUMP_FUNC,      // subexpression becomes this new node. Other arguments discarded.
                COPY,           // node can become copy of another subexpression.
                END,            // helper
            };
            
            advanced_mutation_t() = default;
            
            explicit advanced_mutation_t(const config_t& config): mutation_t(config)
            {}
            
            bool apply(gp_program& program, const tree_t& p, tree_t& c) final;
            
            advanced_mutation_t& set_per_node_mutation_chance(double v)
            {
                per_node_mutation_chance = v;
                return *this;
            }
        
        private:
            static constexpr auto operators_size = static_cast<blt::i32>(mutation_operator::END);
        private:
            // this value is adjusted inversely to the size of the tree.
            double per_node_mutation_chance = 5.0;
            
            static constexpr std::array<double, operators_size> mutation_operator_chances = detail::aggregate_array<operators_size>(
                    0.25,       // EXPRESSION
                    0.15,       // ADJUST
                    0.01,       // SUB_FUNC
                    0.01,       // JUMP_FUNC
                    0.05        // COPY
                                                                                                                                   );
    };
    
}

#endif //BLT_GP_TRANSFORMERS_H
