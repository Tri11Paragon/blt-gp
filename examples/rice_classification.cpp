/*
 *  This rice classification example uses data from the UC Irvine Machine Learning repository.
 *  The data for this example can be found at:
 *  https://archive.ics.uci.edu/dataset/545/rice+cammeo+and+osmancik
 *
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

static const auto SEED_FUNC = [] { return std::random_device()(); };

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

std::vector<rice_record> training_cases;
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
blt::gp::gp_program program{type_system, SEED_FUNC, config};

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
    for (auto& training_case : training_cases)
    {
        auto v = current_tree.get_evaluation_value<float>(&training_case);
        switch (training_case.type)
        {
            case rice_type_t::Cammeo:
                if (v >= 0)
                    fitness.hits++;
                break;
            case rice_type_t::Osmancik:
                if (v < 0)
                    fitness.hits++;
                break;
        }
    }
    fitness.raw_fitness = static_cast<double>(fitness.hits);
    fitness.standardized_fitness = fitness.raw_fitness;
    fitness.adjusted_fitness = 1.0 - (1.0 / (1.0 + fitness.standardized_fitness));
    return static_cast<blt::size_t>(fitness.hits) == training_cases.size();
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
                      std::stof(data[6]), blt::string::contains(data[7], "Cammeo") ? rice_type_t::Cammeo : rice_type_t::Osmancik};
        switch (r.type)
        {
            case rice_type_t::Cammeo:
                c.push_back(r);
                break;
            case rice_type_t::Osmancik:
                o.push_back(r);
                break;
        }
    }
    
    blt::size_t total_records = c.size() + o.size();
    blt::size_t training_size = std::min(total_records / 3, 1000ul);
    for (blt::size_t i = 0; i < training_size; i++)
    {
        auto& random = program.get_random();
        auto& vec = random.choice() ? c : o;
        auto pos = random.get_i64(0, static_cast<blt::i64>(vec.size()));
        training_cases.push_back(vec[pos]);
        vec.erase(vec.begin() + pos);
    }
    testing_cases.insert(testing_cases.end(), c.begin(), c.end());
    testing_cases.insert(testing_cases.end(), o.begin(), o.end());
    std::shuffle(testing_cases.begin(), testing_cases.end(), program.get_random());
    BLT_INFO("Created training set of size %ld, testing set is of size %ld", training_size, testing_cases.size());
}

struct test_results_t
{
    blt::size_t cc = 0;
    blt::size_t co = 0;
    blt::size_t oo = 0;
    blt::size_t oc = 0;
    blt::size_t hits = 0;
    blt::size_t size = 0;
    double percent_hit = 0;
};

test_results_t test_individual(blt::gp::individual& i)
{
    test_results_t results;
    
    for (auto& testing_case : testing_cases)
    {
        auto result = i.tree.get_evaluation_value<float>(&testing_case);
        switch (testing_case.type)
        {
            case rice_type_t::Cammeo:
                if (result >= 0)
                    results.cc++; // cammeo cammeo
                else if (result < 0)
                    results.co++; // cammeo osmancik
                break;
            case rice_type_t::Osmancik:
                if (result < 0)
                    results.oo++; // osmancik osmancik
                else if (result >= 0)
                    results.oc++; // osmancik cammeo
                break;
        }
    }
    
    results.hits = results.cc + results.oo;
    results.size = testing_cases.size();
    results.percent_hit = static_cast<double>(results.hits) / static_cast<double>(results.size) * 100;
    
    return results;
}

int main(int argc, const char** argv)
{
    blt::arg_parse parser;
    parser.addArgument(blt::arg_builder{"-f", "--file"}.setHelp("File for rice data. Should be in .arff format.").setRequired().build());
    
    auto args = parser.parse_args(argc, argv);
    
    if (!args.contains("file"))
    {
        BLT_WARN("Please provide path to file with -f or --file");
        return 1;
    }
    
    auto rice_file_path = args.get<std::string>("file");
    
    BLT_INFO("Starting BLT-GP Rice Classification Example");
    BLT_START_INTERVAL("Rice Classification", "Main");
    BLT_DEBUG("Setup Fitness cases");
    load_rice_data(rice_file_path);
    
    BLT_DEBUG("Setup Types and Operators");
    type_system.register_type<float>();
    
    blt::gp::operator_builder<rice_record> builder{type_system};
    program.set_operations(builder.build(add, sub, mul, pro_div, op_exp, op_log, lit, op_area, op_perimeter, op_major_axis_length,
                                         op_minor_axis_length, op_eccentricity, op_convex_area, op_extent));
    
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
        auto& stats = program.get_population_stats();
        BLT_TRACE("Stats:");
        BLT_TRACE("Average fitness: %lf", stats.average_fitness.load());
        BLT_TRACE("Best fitness: %lf", stats.best_fitness.load());
        BLT_TRACE("Worst fitness: %lf", stats.worst_fitness.load());
        BLT_TRACE("Overall fitness: %lf", stats.overall_fitness.load());

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
        struct match_t
        {
            blt::size_t cc = 0;
            blt::size_t co = 0;
            blt::size_t oo = 0;
            blt::size_t oc = 0;
        };
        
        match_t match;
        
        for (auto& testing_case : testing_cases)
        {
            auto result = i.tree.get_evaluation_value<float>(&testing_case);
            switch (testing_case.type)
            {
                case rice_type_t::Cammeo:
                    if (result >= 0)
                        match.cc++; // cammeo cammeo
                    else if (result < 0)
                        match.co++; // cammeo osmancik
                    break;
                case rice_type_t::Osmancik:
                    if (result < 0)
                        match.oo++; // osmancik osmancik
                    else if (result >= 0)
                        match.oc++; // osmancik cammeo
                    break;
            }
        }
        
        auto hits = match.cc + match.oo;
        auto size = testing_cases.size();
        
        BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", hits, size, static_cast<double>(hits) / static_cast<double>(size) * 100);
        BLT_DEBUG("Cammeo Cammeo: %ld", match.cc);
        BLT_DEBUG("Cammeo Osmancik: %ld", match.co);
        BLT_DEBUG("Osmancik Osmancik: %ld", match.oo);
        BLT_DEBUG("Osmancik Cammeo: %ld", match.oc);
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