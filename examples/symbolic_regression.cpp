/*
 *  <Short Description>
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
#include <blt/gp/program.h>
#include <blt/profiling/profiler_v2.h>
#include <blt/gp/tree.h>
#include <blt/std/logging.h>
#include <blt/std/format.h>
#include <iostream>
#include "operations_common.h"
#include "blt/math/averages.h"

//static constexpr long SEED = 41912;
static const unsigned long SEED = std::random_device()();

struct context
{
    float x, y;
};

std::array<context, 200> training_cases;

blt::gp::prog_config_t config = blt::gp::prog_config_t()
        .set_initial_min_tree_size(2)
        .set_initial_max_tree_size(6)
        .set_elite_count(0)
        .set_crossover_chance(0.9)
        .set_mutation_chance(0.1)
        .set_reproduction_chance(0)
        .set_max_generations(50)
        .set_pop_size(1)
        .set_thread_count(1);

blt::gp::gp_program program{SEED, config};

auto lit = blt::gp::operation_t([]() {
    return program.get_random().get_float(-1.0f, 1.0f);
}, "lit").set_ephemeral();

blt::gp::operation_t op_x([](const context& context) {
    return context.x;
}, "x");

constexpr auto fitness_function = [](blt::gp::tree_t& current_tree, blt::gp::fitness_t& fitness, blt::size_t) {
    constexpr double value_cutoff = 1.e15;
    for (auto& fitness_case : training_cases)
    {
        auto diff = std::abs(fitness_case.y - current_tree.get_evaluation_value<float>(&fitness_case));
        if (diff < value_cutoff)
        {
            fitness.raw_fitness += diff;
            if (diff < 0.01)
                fitness.hits++;
        } else
            fitness.raw_fitness += value_cutoff;
    }
    fitness.standardized_fitness = fitness.raw_fitness;
    fitness.adjusted_fitness = (1.0 / (1.0 + fitness.standardized_fitness));
    return static_cast<blt::size_t>(fitness.hits) == training_cases.size();
};

float example_function(float x)
{
    return x * x * x * x + x * x * x + x * x + x;
}

int main()
{
    BLT_INFO("Starting BLT-GP Symbolic Regression Example");
    BLT_START_INTERVAL("Symbolic Regression", "Main");
    BLT_DEBUG("Setup Fitness cases");
    for (auto& fitness_case : training_cases)
    {
        constexpr float range = 10;
        constexpr float half_range = range / 2.0;
        auto x = program.get_random().get_float(-half_range, half_range);
        auto y = example_function(x);
        fitness_case = {x, y};
    }
    
    BLT_DEBUG("Setup Types and Operators");
    blt::gp::operator_builder<context> builder{};
    program.set_operations(builder.build(add, sub, mul, pro_div, op_sin, op_cos, op_exp, op_log, lit, op_x));
    
    BLT_DEBUG("Generate Initial Population");
    auto sel = blt::gp::select_tournament_t{};
    program.generate_population(program.get_typesystem().get_type<float>().id(), fitness_function, sel, sel, sel);
    
    BLT_DEBUG("Begin Generation Loop");
    while (!program.should_terminate())
    {
        BLT_TRACE("------------{Begin Generation %ld}------------", program.get_current_generation());
        BLT_TRACE("Creating next generation");
        BLT_START_INTERVAL("Symbolic Regression", "Gen");
        program.create_next_generation();
        BLT_END_INTERVAL("Symbolic Regression", "Gen");
        BLT_TRACE("Move to next generation");
        BLT_START_INTERVAL("Symbolic Regression", "Fitness");
        program.next_generation();
        BLT_TRACE("Evaluate Fitness");
        program.evaluate_fitness();
        BLT_END_INTERVAL("Symbolic Regression", "Fitness");
        BLT_TRACE("----------------------------------------------");
        std::cout << std::endl;
    }
    
    BLT_END_INTERVAL("Symbolic Regression", "Main");
    
    auto best = program.get_best_individuals<3>();
    
    BLT_INFO("Best approximations:");
    for (auto& i_ref : best)
    {
        auto& i = i_ref.get();
        BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);
        i.tree.print(program, std::cout);
        std::cout << "\n";
    }
    auto& stats = program.get_population_stats();
    BLT_INFO("Stats:");
    BLT_INFO("Average fitness: %lf", stats.average_fitness.load());
    BLT_INFO("Best fitness: %lf", stats.best_fitness.load());
    BLT_INFO("Worst fitness: %lf", stats.worst_fitness.load());
    BLT_INFO("Overall fitness: %lf", stats.overall_fitness.load());
    // TODO: make stats helper
    
    BLT_PRINT_PROFILE("Symbolic Regression", blt::PRINT_CYCLES | blt::PRINT_THREAD | blt::PRINT_WALL);

#ifdef BLT_TRACK_ALLOCATIONS
    BLT_TRACE("Total Allocations: %ld times with a total of %s, peak allocated bytes %s", blt::gp::tracker.getAllocations(),
              blt::byte_convert_t(blt::gp::tracker.getAllocatedBytes()).convert_to_nearest_type().to_pretty_string().c_str(),
              blt::byte_convert_t(blt::gp::tracker.getPeakAllocatedBytes()).convert_to_nearest_type().to_pretty_string().c_str());
    BLT_TRACE("------------------------------------------------------");
    auto evaluation_calls_v = blt::gp::evaluation_calls.get_calls();
    auto evaluation_allocations_v = blt::gp::evaluation_allocations.get_calls();
    BLT_TRACE("Total Evaluation Calls: %ld; Peak Bytes Allocated %s", evaluation_calls_v,
              blt::string::bytes_to_pretty(blt::gp::evaluation_calls.get_value()).c_str());
    BLT_TRACE("Total Evaluation Allocations: %ld; Bytes %s; Average %s", evaluation_allocations_v,
              blt::string::bytes_to_pretty(blt::gp::evaluation_allocations.get_value()).c_str(),
              blt::string::bytes_to_pretty(blt::average(blt::gp::evaluation_allocations.get_value(), evaluation_allocations_v)).c_str());
    BLT_TRACE("Percent Evaluation calls allocate? %lf%%", blt::average(evaluation_allocations_v, evaluation_calls_v) * 100);
    BLT_TRACE("------------------------------------------------------");
    auto crossover_calls_v = blt::gp::crossover_calls.get_calls();
    auto crossover_allocations_v = blt::gp::crossover_allocations.get_calls();
    auto mutation_calls_v = blt::gp::mutation_calls.get_calls();
    auto mutation_allocations_v = blt::gp::mutation_allocations.get_calls();
    auto reproduction_calls_v = blt::gp::reproduction_calls.get_calls();
    auto reproduction_allocations_v = blt::gp::reproduction_allocations.get_calls();
    BLT_TRACE("Total Crossover Calls: %ld; Peak Bytes Allocated %s", crossover_calls_v,
              blt::string::bytes_to_pretty(blt::gp::crossover_calls.get_value()).c_str());
    BLT_TRACE("Total Mutation Calls: %ld; Peak Bytes Allocated %s", mutation_calls_v,
              blt::string::bytes_to_pretty(blt::gp::mutation_calls.get_value()).c_str());
    BLT_TRACE("Total Reproduction Calls: %ld; Peak Bytes Allocated %s", reproduction_calls_v,
              blt::string::bytes_to_pretty(blt::gp::reproduction_calls.get_value()).c_str());
    BLT_TRACE("Total Crossover Allocations: %ld; Bytes %s; Average %s", crossover_allocations_v,
              blt::string::bytes_to_pretty(blt::gp::crossover_allocations.get_value()).c_str(),
              blt::string::bytes_to_pretty(blt::average(blt::gp::crossover_allocations.get_value(), crossover_allocations_v)).c_str());
    BLT_TRACE("Total Mutation Allocations: %ld; Bytes %s; Average %s", mutation_allocations_v,
              blt::string::bytes_to_pretty(blt::gp::mutation_allocations.get_value()).c_str(),
              blt::string::bytes_to_pretty(blt::average(blt::gp::mutation_allocations.get_value(), mutation_allocations_v)).c_str());
    BLT_TRACE("Total Reproduction Allocations: %ld; Bytes %s; Average %s", reproduction_allocations_v,
              blt::string::bytes_to_pretty(blt::gp::reproduction_allocations.get_value()).c_str(),
              blt::string::bytes_to_pretty(blt::average(blt::gp::reproduction_allocations.get_value(), reproduction_allocations_v)).c_str());
    BLT_TRACE("Percent Crossover calls allocate? %lf%%", blt::average(crossover_allocations_v, crossover_calls_v) * 100);
    BLT_TRACE("Percent Mutation calls allocate? %lf%%", blt::average(mutation_allocations_v, mutation_calls_v) * 100);
    BLT_TRACE("Percent Reproduction calls allocate? %lf%%", blt::average(reproduction_allocations_v, reproduction_calls_v) * 100);
#endif
    
    return 0;
}