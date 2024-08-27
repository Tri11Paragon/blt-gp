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
#include <blt/parse/argparse.h>
#include <iostream>
#include "operations_common.h"
#include "blt/fs/loader.h"


//static constexpr long SEED = 41912;
static const unsigned long SEED = std::random_device()();

enum class rice_type_t
{
    Cammeo,
    Osmancik
};

struct rice_record
{
    float area;
    float perimeter;
    float major_axis_length;
    float minor_axis_length;
    float eccentricity;
    float convex_area;
    float extent;
    rice_type_t type;
};

std::vector<rice_record> fitness_cases;
std::vector<rice_record> testing_cases;

blt::gp::prog_config_t config = blt::gp::prog_config_t()
        .set_initial_min_tree_size(2)
        .set_initial_max_tree_size(6)
        .set_elite_count(2)
        .set_crossover_chance(0.9)
        .set_mutation_chance(0.1)
        .set_reproduction_chance(0)
        .set_max_generations(50)
        .set_pop_size(500)
        .set_thread_count(0);

blt::gp::type_provider type_system;
blt::gp::gp_program program{type_system, SEED, config};

auto lit = blt::gp::operation_t([]() {
    return program.get_random().get_float(-32000.0f, 32000.0f);
}, "lit").set_ephemeral();

blt::gp::operation_t op_area([](const rice_record& rice_data) {
    return rice_data.area;
}, "area");

blt::gp::operation_t op_perimeter([](const rice_record& rice_data) {
    return rice_data.perimeter;
}, "perimeter");

blt::gp::operation_t op_major_axis_length([](const rice_record& rice_data) {
    return rice_data.major_axis_length;
}, "major_axis_length");

blt::gp::operation_t op_minor_axis_length([](const rice_record& rice_data) {
    return rice_data.minor_axis_length;
}, "minor_axis_length");

blt::gp::operation_t op_eccentricity([](const rice_record& rice_data) {
    return rice_data.eccentricity;
}, "eccentricity");

blt::gp::operation_t op_convex_area([](const rice_record& rice_data) {
    return rice_data.convex_area;
}, "convex_area");

blt::gp::operation_t op_extent([](const rice_record& rice_data) {
    return rice_data.extent;
}, "extent");

constexpr auto fitness_function = [](blt::gp::tree_t& current_tree, blt::gp::fitness_t& fitness, blt::size_t) {
    constexpr double value_cutoff = 1.e15;
    for (auto& fitness_case : fitness_cases)
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
    return static_cast<blt::size_t>(fitness.hits) == fitness_cases.size();
};

void load_rice_data(std::string_view rice_file_path)
{
    auto rice_file_data = blt::fs::getLinesFromFile(rice_file_path);
    size_t index = 0;
    while (!blt::string::contains(rice_file_data[index++], "@DATA"))
    {}
    std::vector<rice_record> c;
    std::vector<rice_record> o;
    for (std::string_view v : blt::itr_offset(rice_file_data, index))
    {
        auto data = blt::string::split(v, ',');
        rice_record r{std::stof(data[0]), std::stof(data[1]), std::stof(data[2]), std::stof(data[3]), std::stof(data[4]), std::stof(data[5]),
                      std::stof(data[6])};
        if (blt::string::contains(data[7], "Cammeo"))
        {
            r.type = rice_type_t::Cammeo;
            c.push_back(r);
        } else
        {
            r.type = rice_type_t::Osmancik;
            o.push_back(r);
        }
    }
    blt::size_t total_records = c.size() + o.size();
    blt::size_t training_size = total_records / 3;
    for (blt::size_t i = 0; i < training_size; i++)
    {
        auto& random = program.get_random();
        auto& vec = random.choice() ? c : o;
        auto pos = random.get_i64(0, static_cast<blt::i64>(vec.size()));
        fitness_cases.push_back(vec[pos]);
        vec.erase(vec.begin() + pos);
    }
    testing_cases.insert(testing_cases.end(), c.begin(), c.end());
    testing_cases.insert(testing_cases.end(), o.begin(), o.end());
    std::shuffle(testing_cases.begin(), testing_cases.end(), program.get_random());
}

int main(int argc, const char** argv)
{
    blt::arg_parse parser;
    parser.addArgument(blt::arg_builder{"-f", "--file"}.setHelp("File for rice data. Should be in .arff format.").setRequired().build());
    
    auto args = parser.parse_args(argc, argv);
    
    auto rice_file_path = args.get<std::string>("-f");
    
    BLT_INFO("Starting BLT-GP Rice Classification Example");
    BLT_START_INTERVAL("Rice Classification", "Main");
    BLT_DEBUG("Setup Fitness cases");
    load_rice_data(rice_file_path);
    
    BLT_DEBUG("Setup Types and Operators");
    type_system.register_type<float>();
    
    blt::gp::operator_builder<rice_record> builder{type_system};
    program.set_operations(builder.build(add, sub, mul, pro_div, op_sin, op_cos, op_exp, op_log, lit, op_x));
    
    BLT_DEBUG("Generate Initial Population");
    auto sel = blt::gp::select_tournament_t{};
    program.generate_population(type_system.get_type<float>().id(), fitness_function, sel, sel, sel);
    
    BLT_DEBUG("Begin Generation Loop");
    while (!program.should_terminate())
    {
        BLT_TRACE("------------{Begin Generation %ld}------------", program.get_current_generation());
        BLT_TRACE("Creating next generation");

#ifdef BLT_TRACK_ALLOCATIONS
        auto gen_alloc = blt::gp::tracker.start_measurement();
#endif
        
        BLT_START_INTERVAL("Rice Classification", "Gen");
        program.create_next_generation();
        BLT_END_INTERVAL("Rice Classification", "Gen");

#ifdef BLT_TRACK_ALLOCATIONS
        blt::gp::tracker.stop_measurement(gen_alloc);
        BLT_TRACE("Generation Allocated %ld times with a total of %s", gen_alloc.getAllocationDifference(),
                  blt::byte_convert_t(gen_alloc.getAllocatedByteDifference()).convert_to_nearest_type().to_pretty_string().c_str());
        auto fitness_alloc = blt::gp::tracker.start_measurement();
#endif
        
        BLT_TRACE("Move to next generation");
        BLT_START_INTERVAL("Rice Classification", "Fitness");
        program.next_generation();
        BLT_TRACE("Evaluate Fitness");
        program.evaluate_fitness();
        BLT_END_INTERVAL("Rice Classification", "Fitness");

#ifdef BLT_TRACK_ALLOCATIONS
        blt::gp::tracker.stop_measurement(fitness_alloc);
        BLT_TRACE("Fitness Allocated %ld times with a total of %s", fitness_alloc.getAllocationDifference(),
                  blt::byte_convert_t(fitness_alloc.getAllocatedByteDifference()).convert_to_nearest_type().to_pretty_string().c_str());
#endif
        
        BLT_TRACE("----------------------------------------------");
        std::cout << std::endl;
    }
    
    BLT_END_INTERVAL("Rice Classification", "Main");
    
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
    
    BLT_PRINT_PROFILE("Rice Classification", blt::PRINT_CYCLES | blt::PRINT_THREAD | blt::PRINT_WALL);

#ifdef BLT_TRACK_ALLOCATIONS
    BLT_TRACE("Total Allocations: %ld times with a total of %s", blt::gp::tracker.getAllocations(),
              blt::byte_convert_t(blt::gp::tracker.getAllocatedBytes()).convert_to_nearest_type().to_pretty_string().c_str());
#endif
    
    return 0;
}