/*
 *  <Short Description>
 *  Copyright (C) 2025  Brett Terpstra
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
#include "../examples/symbolic_regression.h"

#include <fstream>
#include <array>
#include <blt/profiling/profiler_v2.h>

static const auto SEED_FUNC = [] { return std::random_device()(); };

std::array crossover_chances = {1.0, 0.9, 0.8, 0.7, 0.6, 0.5, 0.4, 0.3, 0.2, 0.1, 0.0};
std::array mutation_chances = {0.0, 0.1, 0.2, 0.9, 1.0};
std::array reproduction_chances = {0.0, 1.0, 0.1, 0.9};
std::array elite_amounts = {0, 2, 10, 50};
std::array population_sizes = {50, 500, 5000};

blt::gp::prog_config_t best_config;
double best_fitness = 0;

void run(const blt::gp::prog_config_t& config)
{
    // the config is copied into the gp_system so changing the config will not change the runtime of the program.
    blt::gp::example::symbolic_regression_t regression{SEED_FUNC, config};


    BLT_START_INTERVAL("Symbolic Regression", "Setup Operations");
    regression.setup_operations();
    BLT_END_INTERVAL("Symbolic Regression", "Setup Operations");

    BLT_START_INTERVAL("Symbolic Regression", "Generate Initial Population");
    regression.generate_initial_population();
    BLT_END_INTERVAL("Symbolic Regression", "Generate Initial Population");

    BLT_START_INTERVAL("Symbolic Regression", "Total Generation Loop");
    BLT_DEBUG("Begin Generation Loop");
    auto& program = regression.get_program();
    while (!program.should_terminate())
    {
#ifdef BLT_TRACK_ALLOCATIONS
                auto cross = crossover_calls.start_measurement();
                auto mut = mutation_calls.start_measurement();
                auto repo = reproduction_calls.start_measurement();
#endif
        BLT_TRACE("------------{Begin Generation %ld}------------", program.get_current_generation());
        BLT_TRACE("Creating next generation");
        BLT_START_INTERVAL("Symbolic Regression", "Create Next Generation");
        program.create_next_generation();
        BLT_END_INTERVAL("Symbolic Regression", "Create Next Generation");
        BLT_TRACE("Move to next generation");
        BLT_START_INTERVAL("Symbolic Regress", "Move Next Generation");
        program.next_generation();
        BLT_END_INTERVAL("Symbolic Regress", "Move Next Generation");
        BLT_TRACE("Evaluate Fitness");
        BLT_START_INTERVAL("Symbolic Regress", "Evaluate Fitness");
        program.evaluate_fitness();
        BLT_END_INTERVAL("Symbolic Regress", "Evaluate Fitness");
        BLT_START_INTERVAL("Symbolic Regress", "Fitness Print");
        const auto& stats = program.get_population_stats();
        BLT_TRACE("Avg Fit: %lf, Best Fit: %lf, Worst Fit: %lf, Overall Fit: %lf",
                  stats.average_fitness.load(std::memory_order_relaxed), stats.best_fitness.load(std::memory_order_relaxed),
                  stats.worst_fitness.load(std::memory_order_relaxed), stats.overall_fitness.load(std::memory_order_relaxed));
        BLT_END_INTERVAL("Symbolic Regress", "Fitness Print");
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
    BLT_END_INTERVAL("Symbolic Regression", "Total Generation Loop");

    const auto best = program.get_best_individuals<1>();

    if (best[0].get().fitness.adjusted_fitness > best_fitness)
    {
        best_fitness = best[0].get().fitness.adjusted_fitness;
        best_config = config;
    }
}

void do_run()
{
    std::stringstream results;
    for (const auto crossover_chance : crossover_chances)
    {
        for (const auto mutation_chance : mutation_chances)
        {
            for (const auto reproduction_chance : reproduction_chances)
            {
                if (crossover_chance == 0 && mutation_chance == 0 && reproduction_chance == 0)
                    continue;
                for (const auto elite_amount : elite_amounts)
                {
                    for (const auto population_sizes : population_sizes)
                    {
                        blt::gp::prog_config_t config = blt::gp::prog_config_t()
                                                        .set_initial_min_tree_size(2)
                                                        .set_initial_max_tree_size(6)
                                                        .set_elite_count(elite_amount)
                                                        .set_crossover_chance(crossover_chance)
                                                        .set_mutation_chance(mutation_chance)
                                                        .set_reproduction_chance(reproduction_chance)
                                                        .set_max_generations(50)
                                                        .set_pop_size(population_sizes)
                                                        .set_thread_count(0);

                        BLT_INFO_STREAM << "Run: Crossover (";
                        BLT_INFO_STREAM << crossover_chance;
                        BLT_INFO_STREAM << ") Mutation (";
                        BLT_INFO_STREAM << mutation_chance;
                        BLT_INFO_STREAM << ") Reproduction (";
                        BLT_INFO_STREAM << reproduction_chance;
                        BLT_INFO_STREAM << ") Elite (";
                        BLT_INFO_STREAM << elite_amount;
                        BLT_INFO_STREAM << ") Population Size (";
                        BLT_INFO_STREAM << population_sizes;
                        BLT_INFO_STREAM << ")" << "\n";
                        run(config);

                        results << "Run: Crossover (";
                        results << crossover_chance;
                        results << ") Mutation (";
                        results << mutation_chance;
                        results << ") Reproduction (";
                        results << reproduction_chance;
                        results << ") Elite (";
                        results << elite_amount;
                        results << ") Population Size (";
                        results << population_sizes;
                        results << ")" << std::endl;
                        BLT_WRITE_PROFILE(results, "Symbolic Regression");
                        results << std::endl;
                    }
                }
            }
        }
    }
    std::cout << results.str() << std::endl;

    std::cout << "Best Configuration is: " << std::endl;
    std::cout << "\tCrossover: " << best_config.crossover_chance << std::endl;
    std::cout << "\tMutation: " << best_config.mutation_chance << std::endl;
    std::cout << "\tReproduction: " << best_config.reproduction_chance << std::endl;
    std::cout << "\tElites: " << best_config.elites << std::endl;
    std::cout << "\tPopulation Size: " << best_config.population_size << std::endl;
    std::cout << std::endl;
}

int main()
{
    for (int i = 0; i < 1; i++)
        do_run();
    return 0;
}
