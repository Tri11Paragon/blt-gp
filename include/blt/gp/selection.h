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
#include <blt/gp/tree.h>
#include <blt/gp/config.h>
#include <blt/gp/random.h>
#include <blt/std/assert.h>

namespace blt::gp
{
    
    struct selector_args
    {
        gp_program& program;
        population_t& next_pop;
        population_t& current_pop;
        population_stats& current_stats;
        prog_config_t& config;
        random_t& random;
    };
    
    template<typename Crossover, typename Mutation, typename Reproduction>
    constexpr inline auto default_next_pop_creator = [](
            selector_args&& args, Crossover&& crossover_selection, Mutation&& mutation_selection, Reproduction&& reproduction_selection) {
        auto& [program, next_pop, current_pop, current_stats, config, random] = args;
        
        double total_prob = config.mutation_chance + config.crossover_chance;
        double crossover_chance = config.crossover_chance / total_prob;
        double mutation_chance = crossover_chance + config.mutation_chance / total_prob;
        
        if (config.elites > 0)
        {
            std::vector<std::pair<std::size_t, double>> values;
            
            for (blt::size_t i = 0; i < config.elites; i++)
                values.emplace_back(i, current_pop.get_individuals()[i].fitness.adjusted_fitness);
            
            for (const auto& ind : blt::enumerate(current_pop.get_individuals()))
            {
                for (blt::size_t i = 0; i < config.elites; i++)
                {
//                    BLT_INFO("%lf >= %lf? // %lf", ind.second.fitness.adjusted_fitness, values[i].second, ind.second.fitness.raw_fitness);
                    if (ind.second.fitness.adjusted_fitness >= values[i].second)
                    {
                        bool doesnt_contain = true;
                        for (blt::size_t j = 0; j < config.elites; j++)
                        {
                            if (ind.first == values[j].first)
                                doesnt_contain = false;
                        }
                        if (doesnt_contain)
                            values[i] = {ind.first, ind.second.fitness.adjusted_fitness};
                        break;
                    }
                }
            }
            
            for (blt::size_t i = 0; i < config.elites; i++)
            {
//                BLT_DEBUG("%lf at %ld", values[i].second, values[i].first);
                next_pop.get_individuals().push_back(current_pop.get_individuals()[values[i].first]);
            }
        }
        
        while (next_pop.get_individuals().size() < config.population_size)
        {
            auto type = random.get_double();
            if (type > crossover_chance && type < mutation_chance)
            {
                // crossover
                auto& p1 = crossover_selection.select(program, current_pop, current_stats);
                auto& p2 = crossover_selection.select(program, current_pop, current_stats);
                
                auto results = config.crossover.get().apply(program, p1, p2);
                
                // if crossover fails, we can check for mutation on these guys. otherwise straight copy them into the next pop
                if (results)
                {
                    next_pop.get_individuals().emplace_back(std::move(results->child1));
                    // annoying check
                    if (next_pop.get_individuals().size() < config.population_size)
                        next_pop.get_individuals().emplace_back(std::move(results->child2));
                } else
                {
                    if (config.try_mutation_on_crossover_failure && random.choice(config.mutation_chance))
                        next_pop.get_individuals().emplace_back(std::move(config.mutator.get().apply(program, p1)));
                    else
                        next_pop.get_individuals().push_back(individual{p1});
                    // annoying check.
                    if (next_pop.get_individuals().size() < config.population_size)
                    {
                        if (config.try_mutation_on_crossover_failure && random.choice(config.mutation_chance))
                            next_pop.get_individuals().emplace_back(std::move(config.mutator.get().apply(program, p2)));
                        else
                            next_pop.get_individuals().push_back(individual{p2});
                    }
                }
            } else if (type > mutation_chance)
            {
                // mutation
                auto& p = mutation_selection.select(program, current_pop, current_stats);
                next_pop.get_individuals().emplace_back(std::move(config.mutator.get().apply(program, p)));
            } else
            {
                // reproduction
                auto& p = reproduction_selection.select(program, current_pop, current_stats);
                next_pop.get_individuals().push_back(individual{p});
            }
        }
    };
    
    class selection_t
    {
        public:
            /**
             * @param program gp program to select with, used in randoms
             * @param pop population to select from
             * @param stats the populations statistics
             * @return
             */
            virtual tree_t& select(gp_program& program, population_t& pop, population_stats& stats) = 0;
            
            virtual void pre_process(gp_program&, population_t&, population_stats&)
            {}
            
            virtual ~selection_t() = default;
    };
    
    class select_best_t : public selection_t
    {
        public:
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
    class select_worst_t : public selection_t
    {
        public:
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
    class select_random_t : public selection_t
    {
        public:
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
    class select_tournament_t : public selection_t
    {
        public:
            explicit select_tournament_t(blt::size_t selection_size = 3): selection_size(selection_size)
            {
                if (selection_size < 1)
                    BLT_ABORT("Unable to select with this size. Must select at least 1 individual!");
            }
            
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
        
        private:
            const blt::size_t selection_size;
    };
    
    class select_fitness_proportionate_t : public selection_t
    {
        public:
            void pre_process(gp_program& program, population_t& pop, population_stats& stats) final;
            
            tree_t& select(gp_program& program, population_t& pop, population_stats& stats) final;
    };
    
}

#endif //BLT_GP_SELECTION_H
