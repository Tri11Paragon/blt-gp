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

#ifndef BLT_GP_CONFIG_H
#define BLT_GP_CONFIG_H

#include <utility>
#include <thread>
#include <blt/std/types.h>
#include <blt/meta/config_generator.h>
#include <blt/gp/generators.h>
#include <blt/gp/transformers.h>

namespace blt::gp
{
    struct prog_config_t
    {
        size_t population_size = 500;
        size_t max_generations = 50;
        size_t initial_min_tree_size = 2;
        size_t initial_max_tree_size = 6;
        size_t max_tree_depth = 17;

        // percent chance that we will do crossover
        double crossover_chance = 0.8;
        // percent chance that we will do mutation
        double mutation_chance = 0.1;
        // percent chance we will do reproduction (copy individual)
        double reproduction_chance = 0.1;
        // everything else will just be selected

        size_t elites = 0;

        bool try_mutation_on_crossover_failure = true;

        std::reference_wrapper<mutation_t> mutator;
        std::reference_wrapper<crossover_t> crossover;
        std::reference_wrapper<population_initializer_t> pop_initializer;

        blt::size_t threads = std::thread::hardware_concurrency();
        // number of elements each thread should pull per execution. this is for granularity performance and can be optimized for better results!
        blt::size_t evaluation_size = 4;

        // default config (ramped half-and-half init) or for buildering
        prog_config_t();

        // default config with a user specified initializer
        prog_config_t(const std::reference_wrapper<population_initializer_t>& popInitializer); // NOLINT

        prog_config_t(size_t populationSize, const std::reference_wrapper<population_initializer_t>& popInitializer);

        prog_config_t(size_t populationSize); // NOLINT

        prog_config_t& set_pop_size(blt::size_t pop)
        {
            population_size = pop;
            //evaluation_size = (population_size / threads) / 2;
            return *this;
        }

        prog_config_t& set_initial_min_tree_size(blt::size_t size)
        {
            initial_min_tree_size = size;
            return *this;
        }

        prog_config_t& set_initial_max_tree_size(blt::size_t size)
        {
            initial_max_tree_size = size;
            return *this;
        }

        prog_config_t& set_crossover(crossover_t& ref)
        {
            crossover = {ref};
            return *this;
        }

        prog_config_t& set_mutation(mutation_t& ref)
        {
            mutator = {ref};
            return *this;
        }

        prog_config_t& set_initializer(population_initializer_t& ref)
        {
            pop_initializer = ref;
            return *this;
        }

        prog_config_t& set_elite_count(blt::size_t new_elites)
        {
            elites = new_elites;
            return *this;
        }

        prog_config_t& set_crossover_chance(double new_crossover_chance)
        {
            crossover_chance = new_crossover_chance;
            return *this;
        }

        prog_config_t& set_mutation_chance(double new_mutation_chance)
        {
            mutation_chance = new_mutation_chance;
            return *this;
        }

        prog_config_t& set_max_generations(blt::size_t new_max_generations)
        {
            max_generations = new_max_generations;
            return *this;
        }

        prog_config_t& set_try_mutation_on_crossover_failure(bool new_try_mutation_on_crossover_failure)
        {
            try_mutation_on_crossover_failure = new_try_mutation_on_crossover_failure;
            return *this;
        }

        prog_config_t& set_thread_count(blt::size_t t)
        {
            if (t == 0)
                t = std::thread::hardware_concurrency();
            threads = t;
            //evaluation_size = (population_size / threads) / 2;
            return *this;
        }

        prog_config_t& set_evaluation_size(blt::size_t s)
        {
            evaluation_size = s;
            return *this;
        }

        prog_config_t& set_max_tree_depth(const size_t depth)
        {
            max_tree_depth = depth;
            return *this;
        }

        prog_config_t& set_reproduction_chance(double chance)
        {
            reproduction_chance = chance;
            return *this;
        }
    };
}

#endif //BLT_GP_CONFIG_H
