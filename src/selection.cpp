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
#include <blt/gp/selection.h>
#include <blt/gp/program.h>

namespace blt::gp
{
    
    const tree_t& select_best_t::select(gp_program&, const population_t& pop)
    {
        auto& first = pop.get_individuals()[0];
        double best_fitness = first.fitness.adjusted_fitness;
        const tree_t* tree = &first.tree;
        for (auto& ind : pop.get_individuals())
        {
            if (ind.fitness.adjusted_fitness > best_fitness)
            {
                best_fitness = ind.fitness.adjusted_fitness;
                tree = &ind.tree;
            }
        }
        return *tree;
    }
    
    const tree_t& select_worst_t::select(gp_program&, const population_t& pop)
    {
        auto& first = pop.get_individuals()[0];
        double worst_fitness = first.fitness.adjusted_fitness;
        const tree_t* tree = &first.tree;
        for (auto& ind : pop.get_individuals())
        {
            if (ind.fitness.adjusted_fitness < worst_fitness)
            {
                worst_fitness = ind.fitness.adjusted_fitness;
                tree = &ind.tree;
            }
        }
        return *tree;
    }
    
    const tree_t& select_random_t::select(gp_program& program, const population_t& pop)
    {
        return pop.get_individuals()[program.get_random().get_size_t(0ul, pop.get_individuals().size())].tree;
    }
    
    const tree_t& select_tournament_t::select(gp_program& program, const population_t& pop)
    {
        blt::u64 best = program.get_random().get_u64(0, pop.get_individuals().size());
        auto& i_ref = pop.get_individuals();
        for (blt::size_t i = 0; i < selection_size; i++)
        {
            auto sel_point = program.get_random().get_u64(0ul, pop.get_individuals().size());
            if (i_ref[sel_point].fitness.adjusted_fitness > i_ref[best].fitness.adjusted_fitness)
                best = sel_point;
        }
        return i_ref[best].tree;
    }
    
    const tree_t& select_fitness_proportionate_t::select(gp_program& program, const population_t& pop)
    {
        auto& stats = program.get_population_stats();
        auto choice = program.get_random().get_double();
        for (const auto& [index, ref] : blt::enumerate(pop))
        {
            if (index == 0)
            {
                if (choice <= stats.normalized_fitness[index])
                    return ref.tree;
            } else
            {
                if (choice > stats.normalized_fitness[index - 1] && choice <= stats.normalized_fitness[index])
                    return ref.tree;
            }
        }
        BLT_WARN("Unable to find individual_t with fitness proportionate. This should not be a possible code path! (%lf)", choice);
        return pop.get_individuals()[0].tree;
        //BLT_ABORT("Unable to find individual");
    }
}