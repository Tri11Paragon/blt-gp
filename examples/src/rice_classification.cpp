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

    blt::gp::example::rice_classification_t rice_classification{SEED_FUNC, config};

    rice_classification.execute(rice_file_path);

    return 0;
}

void blt::gp::example::rice_classification_t::make_operators()
{
    BLT_DEBUG("Setup Types and Operators");
    static operation_t add{[](const float a, const float b) { return a + b; }, "add"};
    static operation_t sub([](const float a, const float b) { return a - b; }, "sub");
    static operation_t mul([](const float a, const float b) { return a * b; }, "mul");
    static operation_t pro_div([](const float a, const float b) { return b == 0.0f ? 0.0f : a / b; }, "div");
    static operation_t op_exp([](const float a) { return std::exp(a); }, "exp");
    static operation_t op_log([](const float a) { return a == 0.0f ? 0.0f : std::log(a); }, "log");
    static auto lit = operation_t([this]()
    {
        return program.get_random().get_float(-32000.0f, 32000.0f);
    }, "lit").set_ephemeral();

    static operation_t op_area([](const rice_record& rice_data)
    {
        return rice_data.area;
    }, "area");

    static operation_t op_perimeter([](const rice_record& rice_data)
    {
        return rice_data.perimeter;
    }, "perimeter");

    static operation_t op_major_axis_length([](const rice_record& rice_data)
    {
        return rice_data.major_axis_length;
    }, "major_axis_length");

    static operation_t op_minor_axis_length([](const rice_record& rice_data)
    {
        return rice_data.minor_axis_length;
    }, "minor_axis_length");

    static operation_t op_eccentricity([](const rice_record& rice_data)
    {
        return rice_data.eccentricity;
    }, "eccentricity");

    static operation_t op_convex_area([](const rice_record& rice_data)
    {
        return rice_data.convex_area;
    }, "convex_area");

    static operation_t op_extent([](const rice_record& rice_data)
    {
        return rice_data.extent;
    }, "extent");

    operator_builder<rice_record> builder{};
    builder.build(add, sub, mul, pro_div, op_exp, op_log, lit, op_area, op_perimeter, op_major_axis_length,
                  op_minor_axis_length, op_eccentricity, op_convex_area, op_extent);
    program.set_operations(builder.grab());
}

bool blt::gp::example::rice_classification_t::fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t) const
{
    for (auto& training_case : training_cases)
    {
        const auto v = current_tree.get_evaluation_value<float>(training_case);
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
    // fitness.adjusted_fitness = 1.0 - (1.0 / (1.0 + fitness.standardized_fitness));
    fitness.adjusted_fitness = fitness.standardized_fitness / static_cast<double>(training_cases.size());
    return static_cast<size_t>(fitness.hits) == training_cases.size();
}

void blt::gp::example::rice_classification_t::load_rice_data(const std::string_view rice_file_path)
{
    BLT_DEBUG("Setup Fitness cases");
    auto rice_file_data = fs::getLinesFromFile(rice_file_path);
    size_t index = 0;
    while (!string::contains(rice_file_data[index++], "@DATA"))
    {
    }
    std::vector<rice_record> c;
    std::vector<rice_record> o;
    for (const std::string_view v : iterate(rice_file_data).skip(index))
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

    const size_t total_records = c.size() + o.size();
    const size_t testing_size = total_records / 3;
    for (size_t i = 0; i < testing_size; i++)
    {
        auto& random = program.get_random();
        auto& vec = random.choice() ? c : o;
        const auto pos = random.get_i64(0, static_cast<i64>(vec.size()));
        testing_cases.push_back(vec[pos]);
        vec.erase(vec.begin() + pos);
    }
    training_cases.insert(training_cases.end(), c.begin(), c.end());
    training_cases.insert(training_cases.end(), o.begin(), o.end());
    std::shuffle(training_cases.begin(), training_cases.end(), program.get_random());
    BLT_INFO("Created testing set of size %ld, training set is of size %ld", testing_cases.size(), training_cases.size());
}

blt::gp::confusion_matrix_t blt::gp::example::rice_classification_t::test_individual(const individual_t& individual) const
{
    confusion_matrix_t confusion_matrix;
    confusion_matrix.set_name_a("cammeo");
    confusion_matrix.set_name_b("osmancik");

    for (auto& testing_case : testing_cases)
    {
        const auto result = individual.tree.get_evaluation_value<float>(testing_case);
        switch (testing_case.type)
        {
        case rice_type_t::Cammeo:
            if (result >= 0)
                confusion_matrix.is_A_predicted_A(); // cammeo cammeo
            else
                confusion_matrix.is_A_predicted_B(); // cammeo osmancik
            break;
        case rice_type_t::Osmancik:
            if (result < 0)
                confusion_matrix.is_B_predicted_B(); // osmancik osmancik
            else
                confusion_matrix.is_B_predicted_A(); // osmancik cammeo
            break;
        }
    }

    return confusion_matrix;
}
