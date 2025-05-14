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

        bool fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t) const;

    public:
        template <typename SEED>
        rice_classification_t(SEED&& seed, const prog_config_t& config): example_base_t{std::forward<SEED>(seed), config}
        {
            BLT_INFO("Starting BLT-GP Rice Classification Example");
            fitness_function_ref = [this](const tree_t& t, fitness_t& f, const size_t i)
            {
                return fitness_function(t, f, i);
            };
        }

        void make_operators();

        void load_rice_data(std::string_view rice_file_path);

        [[nodiscard]] confusion_matrix_t test_individual(const individual_t& individual) const;

        void execute(const std::string_view rice_file_path)
        {
            load_rice_data(rice_file_path);
            make_operators();
            generate_initial_population();
            run_generation_loop();
            evaluate_individuals();
            print_best();
            print_average();
        }

        void run_generation_loop()
        {
            BLT_DEBUG("Begin Generation Loop");
            while (!program.should_terminate())
            {
                BLT_TRACE("------------\\{Begin Generation {}}------------", program.get_current_generation());
                BLT_TRACE("Creating next generation");
                program.create_next_generation();
                BLT_TRACE("Move to next generation");
                program.next_generation();
                BLT_TRACE("Evaluate Fitness");
                program.evaluate_fitness();
                auto& stats = program.get_population_stats();
                BLT_TRACE("Avg Fit: %lf, Best Fit: %lf, Worst Fit: %lf, Overall Fit: %lf",
                          stats.average_fitness.load(std::memory_order_relaxed), stats.best_fitness.load(std::memory_order_relaxed),
                          stats.worst_fitness.load(std::memory_order_relaxed), stats.overall_fitness.load(std::memory_order_relaxed));
                BLT_TRACE("----------------------------------------------");
                std::cout << std::endl;
            }
        }

        void evaluate_individuals()
        {
            results.clear();
            for (auto& i : program.get_current_pop().get_individuals())
                results.emplace_back(test_individual(i), &i);
            std::sort(results.begin(), results.end(), [](const auto& a, const auto& b)
            {
                return a.first > b.first;
            });
        }

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
            program.generate_initial_population(program.get_typesystem().get_type<float>().id());
            program.setup_generational_evaluation(fitness_function_ref, *crossover_sel, *mutation_sel, *reproduction_sel);
        }

        void print_best(const size_t amount = 3)
        {
            BLT_INFO("Best results:");
            for (size_t index = 0; index < amount; index++)
            {
                const auto& record = results[index].first;
                const auto& i = *results[index].second;

                BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", record.get_hits(), record.get_total(), record.get_percent_hit());
                std::cout << record.pretty_print() << std::endl;
                BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);
                i.tree.print(std::cout);

                std::cout << "\n";
            }
        }

        void print_worst(const size_t amount = 3) const
        {
            BLT_INFO("Worst Results:");
            for (size_t index = 0; index < amount; index++)
            {
                const auto& record = results[results.size() - 1 - index].first;
                const auto& i = *results[results.size() - 1 - index].second;

                BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", record.get_hits(), record.get_total(), record.get_percent_hit());
                std::cout << record.pretty_print() << std::endl;
                BLT_DEBUG("Fitness: %lf, stand: %lf, raw: %lf", i.fitness.adjusted_fitness, i.fitness.standardized_fitness, i.fitness.raw_fitness);

                std::cout << "\n";
            }
        }

        void print_average()
        {
            BLT_INFO("Average Results");
            confusion_matrix_t avg{};
            avg.set_name_a("cammeo");
            avg.set_name_b("osmancik");
            for (const auto& [matrix, _] : results)
                avg += matrix;
            avg /= results.size();
            BLT_INFO("Hits %ld, Total Cases %ld, Percent Hit: %lf", avg.get_hits(), avg.get_total(), avg.get_percent_hit());
            std::cout << avg.pretty_print() << std::endl;
            std::cout << "\n";
        }

        auto& get_results() { return results; }
        const auto& get_results() const { return results; }

    private:
        std::vector<rice_record> training_cases;
        std::vector<rice_record> testing_cases;
        std::vector<std::pair<confusion_matrix_t, individual_t*>> results;
    };
}

#endif //BLT_GP_EXAMPLES_RICE_CLASSIFICATION_H
