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

#ifndef BLT_GP_SELECTION_H
#define BLT_GP_SELECTION_H

#include <blt/gp/fwdecl.h>
#include <blt/gp/util/statistics.h>
#include <blt/gp/tree.h>
#include <blt/gp/config.h>
#include <blt/gp/random.h>
#include <blt/std/assert.h>
#include "blt/format/format.h"
#include <atomic>

namespace blt::gp
{
    struct selector_args
    {
        gp_program& program;
        const population_t& current_pop;
        population_stats& current_stats;
        prog_config_t& config;
        random_t& random;
    };

    namespace detail
    {
        constexpr inline auto perform_elitism = [](const selector_args& args, population_t& next_pop)
        {
            auto& [program, current_pop, current_stats, config, random] = args;

            BLT_ASSERT_MSG(config.elites <= current_pop.get_individuals().size(), ("Not enough individuals in population (" +
                               std::to_string(current_pop.get_individuals().size()) +
                               ") for requested amount of elites (" + std::to_string(config.elites) + ")").c_str());

            if (config.elites > 0 && current_pop.get_individuals().size() >= config.elites)
            {
                thread_local tracked_vector<std::pair<std::size_t, double>> values;
                values.clear();

                for (size_t i = 0; i < config.elites; i++)
                    values.emplace_back(i, current_pop.get_individuals()[i].fitness.adjusted_fitness);

                for (const auto& ind : blt::enumerate(current_pop.get_individuals()))
                {
                    for (size_t i = 0; i < config.elites; i++)
                    {
                        if (ind.value.fitness.adjusted_fitness >= values[i].second)
                        {
                            bool doesnt_contain = true;
                            for (blt::size_t j = 0; j < config.elites; j++)
                            {
                                if (ind.index == values[j].first)
                                    doesnt_contain = false;
                            }
                            if (doesnt_contain)
                                values[i] = {ind.index, ind.value.fitness.adjusted_fitness};
                            break;
                        }
                    }
                }

                for (size_t i = 0; i < config.elites; i++)
                    next_pop.get_individuals()[i].copy_fast(current_pop.get_individuals()[values[i].first].tree);
                return config.elites;
            }
            return 0ul;
        };

    }

    class selection_t
    {
    public:
        /**
         * @param program gp program to select with, used in randoms
         * @param pop population to select from
         * @param stats the populations statistics
         * @return
         */
        virtual const tree_t& select(gp_program& program, const population_t& pop) = 0;

        /**
         * Is run once on a single thread before selection begins. allows you to preprocess the generation for fitness metrics.
         * TODO a method for parallel execution
         */
        virtual void pre_process(gp_program&, population_t&)
        {
        }

        virtual ~selection_t() = default;
    };

    class select_best_t final : public selection_t
    {
    public:
        void pre_process(gp_program&, population_t&) override;

        const tree_t& select(gp_program& program, const population_t& pop) override;

    private:
        std::atomic_uint64_t index = 0;
    };

    class select_worst_t final : public selection_t
    {
    public:
        void pre_process(gp_program&, population_t&) override;

        const tree_t& select(gp_program& program, const population_t& pop) override;

    private:
        std::atomic_uint64_t index = 0;
    };

    class select_random_t final : public selection_t
    {
    public:
        const tree_t& select(gp_program& program, const population_t& pop) override;
    };

    class select_tournament_t final : public selection_t
    {
    public:
        explicit select_tournament_t(const size_t selection_size = 3): selection_size(selection_size)
        {
            if (selection_size == 0)
                BLT_ABORT("Unable to select with this size. Must select at least 1 individual_t!");
        }

        const tree_t& select(gp_program& program, const population_t& pop) override;

    private:
        const size_t selection_size;
    };

    class select_fitness_proportionate_t final : public selection_t
    {
    public:
        const tree_t& select(gp_program& program, const population_t& pop) override;
    };
}

#endif //BLT_GP_SELECTION_H
