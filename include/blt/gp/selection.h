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
#include "blt/std/format.h"

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
    
    constexpr inline auto perform_elitism = [](const selector_args& args, population_t& next_pop) {
        auto& [program, current_pop, current_stats, config, random] = args;
        
        if (config.elites > 0 && current_pop.get_individuals().size() >= config.elites)
        {
            static thread_local tracked_vector<std::pair<std::size_t, double>> values;
            values.clear();
            
            for (blt::size_t i = 0; i < config.elites; i++)
                values.emplace_back(i, current_pop.get_individuals()[i].fitness.adjusted_fitness);
            
            for (const auto& ind : blt::enumerate(current_pop.get_individuals()))
            {
                for (blt::size_t i = 0; i < config.elites; i++)
                {
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
                next_pop.get_individuals()[i].copy_fast(current_pop.get_individuals()[values[i].first].tree);
            return config.elites;
        }
        return 0ul;
    };
    
    template<typename Crossover, typename Mutation, typename Reproduction>
    constexpr inline auto default_next_pop_creator = [](
            blt::gp::selector_args& args, Crossover& crossover_selection, Mutation& mutation_selection, Reproduction& reproduction_selection,
            tree_t& c1, tree_t* c2) {
        auto& [program, current_pop, current_stats, config, random] = args;
        
        int sel = random.get_i32(0, 3);
        switch (sel)
        {
            case 0:
                if (c2 == nullptr)
                    return 0;
                // everyone gets a chance once per loop.
                if (random.choice(config.crossover_chance))
                {
#ifdef BLT_TRACK_ALLOCATIONS
                    auto state = tracker.start_measurement_thread_local();
#endif
                    // crossover
                    const tree_t* p1;
                    const tree_t* p2;
                    do
                    {
                        p1 = &crossover_selection.select(program, current_pop);
                        p2 = &crossover_selection.select(program, current_pop);
                        c1.copy_fast(*p1);
                        c2->copy_fast(*p2);
                    } while (!config.crossover.get().apply(program, *p1, *p2, c1, *c2));
#ifdef BLT_TRACK_ALLOCATIONS
                    tracker.stop_measurement_thread_local(state);
                    crossover_calls.call();
                    crossover_calls.set_value(std::max(crossover_calls.get_value(), state.getAllocatedByteDifference()));
                    if (state.getAllocatedByteDifference() != 0)
                    {
                        crossover_allocations.call(state.getAllocatedByteDifference());
                    }
#endif
                    return 2;
                }
                break;
            case 1:
                if (random.choice(config.mutation_chance))
                {
#ifdef BLT_TRACK_ALLOCATIONS
                    auto state = tracker.start_measurement_thread_local();
#endif
                    // mutation
                    const tree_t* p;
                    do
                    {
                        p = &mutation_selection.select(program, current_pop);
                        c1.copy_fast(*p);
                    } while (!config.mutator.get().apply(program, *p, c1));
#ifdef BLT_TRACK_ALLOCATIONS
                    tracker.stop_measurement_thread_local(state);
                    mutation_calls.call();
                    mutation_calls.set_value(std::max(mutation_calls.get_value(), state.getAllocatedByteDifference()));
                    if (state.getAllocationDifference() != 0)
                    {
                        mutation_allocations.call(state.getAllocatedByteDifference());
                    }
#endif
                    return 1;
                }
                break;
            case 2:
                if (config.reproduction_chance > 0 && random.choice(config.reproduction_chance))
                {
#ifdef BLT_TRACK_ALLOCATIONS
                    auto state = tracker.start_measurement_thread_local();
#endif
                    // reproduction
                    c1.copy_fast(reproduction_selection.select(program, current_pop));
#ifdef BLT_TRACK_ALLOCATIONS
                    tracker.stop_measurement_thread_local(state);
                    reproduction_calls.call();
                    reproduction_calls.set_value(std::max(reproduction_calls.get_value(), state.getAllocatedByteDifference()));
                    if (state.getAllocationDifference() != 0)
                    {
                        reproduction_allocations.call(state.getAllocatedByteDifference());
                    }
#endif
                    return 1;
                }
                break;
            default:
#if BLT_DEBUG_LEVEL > 0
                BLT_ABORT("This is not possible!");
#else
                BLT_UNREACHABLE;
#endif
        }
        return 0;
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
            virtual const tree_t& select(gp_program& program, const population_t& pop) = 0;
            
            virtual void pre_process(gp_program&, population_t&)
            {}
            
            virtual ~selection_t() = default;
    };
    
    class select_best_t : public selection_t
    {
        public:
            const tree_t& select(gp_program& program, const population_t& pop) final;
    };
    
    class select_worst_t : public selection_t
    {
        public:
            const tree_t& select(gp_program& program, const population_t& pop) final;
    };
    
    class select_random_t : public selection_t
    {
        public:
            const tree_t& select(gp_program& program, const population_t& pop) final;
    };
    
    class select_tournament_t : public selection_t
    {
        public:
            explicit select_tournament_t(blt::size_t selection_size = 3): selection_size(selection_size)
            {
                if (selection_size == 0)
                    BLT_ABORT("Unable to select with this size. Must select at least 1 individual_t!");
            }
            
            const tree_t& select(gp_program& program, const population_t& pop) final;
        
        private:
            const blt::size_t selection_size;
    };
    
    class select_fitness_proportionate_t : public selection_t
    {
        public:
            const tree_t& select(gp_program& program, const population_t& pop) final;
    };
    
}

#endif //BLT_GP_SELECTION_H
