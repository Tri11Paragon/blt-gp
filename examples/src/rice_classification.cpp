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
#include <blt/format/format.h>
#include <blt/parse/argparse.h>
#include <iostream>
#include "../rice_classification.h"
#include "blt/fs/loader.h"

static const auto SEED_FUNC = [] { return std::random_device()(); };

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

void blt::gp::example::rice_classification_t::load_rice_data(std::string_view rice_file_path)
{
    auto rice_file_data = fs::getLinesFromFile(rice_file_path);
    size_t index = 0;
    while (!string::contains(rice_file_data[index++], "@DATA"))
    {
    }
    std::vector<rice_record> c;
    std::vector<rice_record> o;
    for (std::string_view v : iterate(rice_file_data).skip(index))
    {
        auto data = string::split(v, ',');
        rice_record r{
            std::stof(data[0]), std::stof(data[1]), std::stof(data[2]), std::stof(data[3]), std::stof(data[4]), std::stof(data[5]),
            std::stof(data[6]), string::contains(data[7], "Cammeo") ? rice_type_t::Cammeo : rice_type_t::Osmancik
        };
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

    size_t total_records = c.size() + o.size();
    size_t training_size = std::min(total_records / 3, 1000ul);
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

    test_results_t& operator+=(const test_results_t& a)
    {
        cc += a.cc;
        co += a.co;
        oo += a.oo;
        oc += a.oc;
        hits += a.hits;
        size += a.size;
        percent_hit += a.percent_hit;
        return *this;
    }

    test_results_t& operator/=(blt::size_t s)
    {
        cc /= s;
        co /= s;
        oo /= s;
        oc /= s;
        hits /= s;
        size /= s;
        percent_hit /= static_cast<double>(s);
        return *this;
    }

    friend bool operator<(const test_results_t& a, const test_results_t& b)
    {
        return a.hits < b.hits;
    }

    friend bool operator>(const test_results_t& a, const test_results_t& b)
    {
        return a.hits > b.hits;
    }
};

test_results_t test_individual(blt::gp::individual_t& i)
{
    test_results_t results;

    for (auto& testing_case : testing_cases)
    {
        auto result = i.tree.get_evaluation_value<float>(testing_case);
        switch (testing_case.type)
        {
        case rice_type_t::Cammeo:
            if (result >= 0)
                results.cc++; // cammeo cammeo
            else
                results.co++; // cammeo osmancik
            break;
        case rice_type_t::Osmancik:
            if (result < 0)
                results.oo++; // osmancik osmancik
            else
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

    blt::gp::operator_builder<rice_record> builder{};
    program.set_operations(builder.build(add, sub, mul, pro_div, op_exp, op_log, lit, op_area, op_perimeter, op_major_axis_length,
                                         op_minor_axis_length, op_eccentricity, op_convex_area, op_extent));

    BLT_DEBUG("Generate Initial Population");
    auto sel = blt::gp::select_tournament_t{};
    program.generate_population(program.get_typesystem().get_type<float>().id(), fitness_function, sel, sel, sel);

    BLT_DEBUG("Begin Generation Loop");
    while (!program.should_terminate())
    {
        BLT_TRACE("------------{Begin Generation %ld}------------", program.get_current_generation());
        BLT_TRACE("Creating next generation");
        BLT_START_INTERVAL("Rice Classification", "Gen");
        program.create_next_generation();
        BLT_END_INTERVAL("Rice Classification", "Gen");
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
        BLT_TRACE("----------------------------------------------");
        std::cout << std::endl;
    }

    BLT_END_INTERVAL("Rice Classification", "Main");

    std::vector<std::pair<test_results_t, blt::gp::individual_t*>> results;
    for (auto& i : program.get_current_pop().get_individuals())
        results.emplace_back(test_individual(i), &i);
    std::sort(results.begin(), results.end(), [](const auto& a, const auto& b)
    {
        return a.first > b.first;
    });

    BLT_INFO("Best results:");
    for (blt::size_t index = 0; index < 3; index++)
    {
        const auto& record = results[index].first;
        const auto& i = *results[index].second;

        BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", record.hits, record.size, record.percent_hit);
        BLT_DEBUG("Cammeo Cammeo: %ld", record.cc);
        BLT_DEBUG("Cammeo Osmancik: %ld", record.co);
        BLT_DEBUG("Osmancik Osmancik: %ld", record.oo);
        BLT_DEBUG("Osmancik Cammeo: %ld", record.oc);
        BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);
        i.tree.print(program, std::cout);

        std::cout << "\n";
    }

    BLT_INFO("Worst Results:");
    for (blt::size_t index = 0; index < 3; index++)
    {
        const auto& record = results[results.size() - 1 - index].first;
        const auto& i = *results[results.size() - 1 - index].second;

        BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", record.hits, record.size, record.percent_hit);
        BLT_DEBUG("Cammeo Cammeo: %ld", record.cc);
        BLT_DEBUG("Cammeo Osmancik: %ld", record.co);
        BLT_DEBUG("Osmancik Osmancik: %ld", record.oo);
        BLT_DEBUG("Osmancik Cammeo: %ld", record.oc);
        BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);

        std::cout << "\n";
    }

    BLT_INFO("Average Results");
    test_results_t avg{};
    for (const auto& v : results)
        avg += v.first;
    avg /= results.size();
    BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", avg.hits, avg.size, avg.percent_hit);
    BLT_DEBUG("Cammeo Cammeo: %ld", avg.cc);
    BLT_DEBUG("Cammeo Osmancik: %ld", avg.co);
    BLT_DEBUG("Osmancik Osmancik: %ld", avg.oo);
    BLT_DEBUG("Osmancik Cammeo: %ld", avg.oc);
    std::cout << "\n";

    BLT_PRINT_PROFILE("Rice Classification", blt::PRINT_CYCLES | blt::PRINT_THREAD | blt::PRINT_WALL);

#ifdef BLT_TRACK_ALLOCATIONS
    BLT_TRACE("Total Allocations: %ld times with a total of %s", blt::gp::tracker.getAllocations(),
              blt::byte_convert_t(blt::gp::tracker.getAllocatedBytes()).convert_to_nearest_type().to_pretty_string().c_str());
#endif

    return 0;
}
