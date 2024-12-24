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

#ifndef BLT_GP_EXAMPLES_RICE_CLASSIFICATION_H
#define BLT_GP_EXAMPLES_RICE_CLASSIFICATION_H

#include "examples_base.h"

namespace blt::gp::example
{
    class rice_classification_t : public example_base_t
    {
    private:
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

        void make_operators()
        {
            static operation_t add{[](const float a, const float b) { return a + b; }, "add"};
            static operation_t sub([](const float a, const float b) { return a - b; }, "sub");
            static operation_t mul([](const float a, const float b) { return a * b; }, "mul");
            static operation_t pro_div([](const float a, const float b) { return b == 0.0f ? 1.0f : a / b; }, "div");
            static operation_t op_sin([](const float a) { return std::sin(a); }, "sin");
            static operation_t op_cos([](const float a) { return std::cos(a); }, "cos");
            static operation_t op_exp([](const float a) { return std::exp(a); }, "exp");
            static operation_t op_log([](const float a) { return a == 0.0f ? 0.0f : std::log(a); }, "log");
            static auto lit = blt::gp::operation_t([]()
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
        }

        bool fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t) const
        {
            for (auto& training_case : training_cases)
            {
                auto v = current_tree.get_evaluation_value<float>(training_case);
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
            return static_cast<size_t>(fitness.hits) == training_cases.size();
        }

        void load_rice_data(std::string_view rice_file_path);

    public:
        template <typename SEED>
        rice_classification_t(SEED&& seed, const prog_config_t& config): example_base_t{std::forward<SEED>(seed), config}
        {
            fitness_function_ref = [this](const tree_t& t, fitness_t& f, const size_t i)
            {
                return fitness_function(t, f, i);
            };
        }

    private:
        std::vector<rice_record> training_cases;
        std::vector<rice_record> testing_cases;
    };
}

#endif //BLT_GP_EXAMPLES_RICE_CLASSIFICATION_H
