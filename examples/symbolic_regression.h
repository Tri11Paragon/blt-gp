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

#ifndef BLT_GP_EXAMPLE_SYMBOLIC_REGRESSION_H
#define BLT_GP_EXAMPLE_SYMBOLIC_REGRESSION_H

#include "examples_base.h"
#include <blt/std/logging.h>
#include <blt/format/format.h>
#include <iostream>

namespace blt::gp::example
{
    class symbolic_regression_t : public example_base_t
    {
    public:
        struct context
        {
            float x, y;
        };

    private:
        bool fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t) const;

        static float example_function(const float x)
        {
            return x * x * x * x + x * x * x + x * x + x;
        }

    public:
        template <typename SEED>
        symbolic_regression_t(SEED seed, const prog_config_t& config): example_base_t{std::forward<SEED>(seed), config}
        {
            BLT_INFO("Starting BLT-GP Symbolic Regression Example");
            BLT_DEBUG("Setup Fitness cases");
            for (auto& fitness_case : training_cases)
            {
                constexpr float range = 10;
                constexpr float half_range = range / 2.0;
                const auto x = program.get_random().get_float(-half_range, half_range);
                const auto y = example_function(x);
                fitness_case = {x, y};
            }

            fitness_function_ref = [this](const tree_t& t, fitness_t& f, const size_t i)
            {
                return fitness_function(t, f, i);
            };
        }

        void setup_operations();

        void generate_initial_population()
        {
            BLT_DEBUG("Generate Initial Population");
            static auto sel = select_tournament_t{};
            if (crossover_sel == nullptr)
                crossover_sel = &sel;
            if (mutation_sel == nullptr)
                mutation_sel = &sel;
            if (reproduction_sel == nullptr)
                reproduction_sel = &sel;
            program.generate_population(program.get_typesystem().get_type<float>().id(), fitness_function_ref, *crossover_sel, *mutation_sel,
                                        *reproduction_sel);
        }

        void run_generation_loop()
        {
            BLT_DEBUG("Begin Generation Loop");
            while (!program.should_terminate())
            {
#ifdef BLT_TRACK_ALLOCATIONS
                auto cross = crossover_calls.start_measurement();
                auto mut = mutation_calls.start_measurement();
                auto repo = reproduction_calls.start_measurement();
#endif
                BLT_TRACE("------------{Begin Generation %ld}------------", program.get_current_generation());
                BLT_TRACE("Creating next generation");
                program.create_next_generation();
                BLT_TRACE("Move to next generation");
                program.next_generation();
                BLT_TRACE("Evaluate Fitness");
                program.evaluate_fitness();
                const auto& stats = program.get_population_stats();
                BLT_TRACE("Avg Fit: %lf, Best Fit: %lf, Worst Fit: %lf, Overall Fit: %lf",
                          stats.average_fitness.load(std::memory_order_relaxed), stats.best_fitness.load(std::memory_order_relaxed),
                          stats.worst_fitness.load(std::memory_order_relaxed), stats.overall_fitness.load(std::memory_order_relaxed));
#ifdef BLT_TRACK_ALLOCATIONS
                crossover_calls.stop_measurement(cross);
                mutation_calls.stop_measurement(mut);
                reproduction_calls.stop_measurement(repo);
                const auto total = (cross.get_call_difference() * 2) + mut.get_call_difference() + repo.get_call_difference();
                BLT_TRACE("Calls Crossover: %ld, Mutation %ld, Reproduction %ld; %ld", cross.get_call_difference(), mut.get_call_difference(), repo.get_call_difference(), total);
                BLT_TRACE("Value Crossover: %ld, Mutation %ld, Reproduction %ld; %ld", cross.get_value_difference(), mut.get_value_difference(), repo.get_value_difference(), (cross.get_value_difference() * 2 + mut.get_value_difference() + repo.get_value_difference()) - total);
#endif
                BLT_TRACE("----------------------------------------------");
                std::cout << std::endl;
            }
        }

        auto get_and_print_best()
        {
            const auto best = program.get_best_individuals<3>();

            BLT_INFO("Best approximations:");
            for (auto& i_ref : best)
            {
                auto& i = i_ref.get();
                BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);
                i.tree.print(program, std::cout);
                std::cout << "\n";
            }

            return best;
        }

        void print_stats() const
        {
            // TODO: make stats helper
            const auto& stats = program.get_population_stats();
            BLT_INFO("Stats:");
            BLT_INFO("Average fitness: %lf", stats.average_fitness.load());
            BLT_INFO("Best fitness: %lf", stats.best_fitness.load());
            BLT_INFO("Worst fitness: %lf", stats.worst_fitness.load());
            BLT_INFO("Overall fitness: %lf", stats.overall_fitness.load());
        }

        void execute()
        {
            setup_operations();

            generate_initial_population();

            run_generation_loop();

            get_and_print_best();

            print_stats();
        }

    private:
        std::array<context, 200> training_cases{};
    };
}

#endif //BLT_GP_EXAMPLE_SYMBOLIC_REGRESSION_H
