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

#include <blt/gp/fwdecl.h>
#include <blt/gp/tree.h>
#include <blt/gp/generators.h>
#include <blt/std/expected.h>
#include <blt/meta/config_generator.h>

namespace blt::gp
{
    namespace detail
    {
        template <typename T>
        inline static constexpr double sum(const T& array)
        {
            double init = 0.0;
            for (const double i : array)
                init += i;
            return init;
        }

        template <size_t size, typename... Args>
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
            ptrdiff_t point;
            operator_info_t& type_operator_info;
        };

        struct config_t
        {
            // number of times crossover will try to pick a valid point in the tree. this is purely based on the return type of the operators
            u32 max_crossover_tries = 5;
            // how many times the crossover function can fail before we will skip this operation.
            u32 max_crossover_iterations = 10;
            // if tree have fewer nodes than this number, they will not be considered for crossover
            // should be at least 5 as crossover will not select the root node.
            u32 min_tree_size = 5;
            // used by the traverse version of get_crossover_point
            // at each depth level, what chance do we have to exit with this as our point? or in other words what's the chance we continue traversing
            // this is what this option configures.
            f32 depth_multiplier = 0.5;
            // how often should we select terminals over functions. By default, we only allow selection of terminals 10% of the time
            // this applies to both types of crossover point functions. Traversal will use the parent if it should not pick a terminal.
            f32 terminal_chance = 0.1;
            // use traversal to select point instead of random selection
            bool traverse = false;

            BLT_MAKE_SETTER_LVALUE(u32, max_crossover_tries);
            BLT_MAKE_SETTER_LVALUE(u32, max_crossover_iterations);
            BLT_MAKE_SETTER_LVALUE(u32, min_tree_size);
            BLT_MAKE_SETTER_LVALUE(f32, depth_multiplier);
            BLT_MAKE_SETTER_LVALUE(f32, terminal_chance);
            BLT_MAKE_SETTER_LVALUE(bool, traverse);
        };

        explicit crossover_t(const config_t& config): config(config)
        {
        }

        /**
         * Apply crossover to a set of parents. Note: c1 and c2 are already filled with thier respective parent's elements.
         * @return true if the crossover succeeded, otherwise return false will erase progress.
         */
        virtual bool apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2) = 0;

        [[nodiscard]] const config_t& get_config() const
        {
            return config;
        }

        virtual ~crossover_t() = default;

    protected:
        config_t config;
    };

    /**
     * Base class for crossover which performs basic subtree crossover on two random nodes in the parent tree
     */
    class subtree_crossover_t : public crossover_t
    {
    public:
        struct crossover_point_t
        {
            subtree_point_t p1_crossover_point;
            subtree_point_t p2_crossover_point;
        };


        subtree_crossover_t(): crossover_t(config_t{})
        {
        }

        explicit subtree_crossover_t(const config_t& config): crossover_t(config)
        {
        }

        [[nodiscard]] std::optional<crossover_point_t> get_crossover_point(const tree_t& c1, const tree_t& c2) const;

        [[nodiscard]] std::optional<crossover_point_t> get_crossover_point_traverse(const tree_t& c1, const tree_t& c2) const;

        /**
         * child1 and child2 are copies of the parents, the result of selecting a crossover point and performing standard subtree crossover.
         * the parents are not modified during this process
         * @param program reference to the global program container responsible for managing these trees
         * @param p1 reference to the first parent
         * @param p2 reference to the second parent
         * @param c1 reference to output child 1
         * @param c2 reference to output child 2
         * @return true if function succeeded, otherwise false
         */
        virtual bool apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2) override; // NOLINT

        ~subtree_crossover_t() override = default;

    protected:
        [[nodiscard]] std::optional<subtree_point_t> get_point_traverse_retry(const tree_t& t, std::optional<type_id> type) const;
    };

    class one_point_crossover_t : public crossover_t
    {
    public:
        one_point_crossover_t(): crossover_t(config_t{})
        {
        }

        explicit one_point_crossover_t(const config_t& config): crossover_t(config)
        {
        }

        bool apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2) override;
    };

    class advanced_crossover_t : public crossover_t
    {
        advanced_crossover_t(): crossover_t(config_t{})
        {
        }
    public:
        bool apply(gp_program& program, const tree_t& p1, const tree_t& p2, tree_t& c1, tree_t& c2) override;
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
            {
            }

            config_t();
        };

        mutation_t() = default;

        explicit mutation_t(const config_t& config): config(config)
        {
        }

        virtual bool apply(gp_program& program, const tree_t& p, tree_t& c);

        // returns the point after the mutation
        size_t mutate_point(gp_program& program, tree_t& c, subtree_point_t node) const;

        virtual ~mutation_t() = default;

    protected:
        config_t config;
    };

    class advanced_mutation_t : public mutation_t
    {
    public:
        enum class mutation_operator : i32
        {
            EXPRESSION, // Generate a new random expression
            ADJUST, // adjust the value of the type. (if it is a function it will mutate it to a different one)
            SUB_FUNC, // subexpression becomes argument to new random function. Other args are generated.
            JUMP_FUNC, // subexpression becomes this new node. Other arguments discarded.
            COPY, // node can become copy of another subexpression.
            END, // helper
        };

        advanced_mutation_t() = default;

        explicit advanced_mutation_t(const config_t& config): mutation_t(config)
        {
        }

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
            0.25, // EXPRESSION
            0.20, // ADJUST
            0.05, // SUB_FUNC
            0.15, // JUMP_FUNC
            0.10 // COPY
        );
    };
}

#endif //BLT_GP_TRANSFORMERS_H
