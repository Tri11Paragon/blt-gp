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
    
    tree_t& select_best_t::select(gp_program&, population_t& pop, population_stats&)
    {
        auto& first = pop.get_individuals()[0];
        double best_fitness = first.standardized_fitness;
        tree_t* tree = &first.tree;
        for (auto& ind : pop.get_individuals())
        {
            if (ind.standardized_fitness < best_fitness)
            {
                best_fitness = ind.standardized_fitness;
                tree = &ind.tree;
            }
        }
        return *tree;
    }
    
    tree_t& select_worst_t::select(gp_program&, population_t& pop, population_stats&)
    {
        auto& first = pop.get_individuals()[0];
        double worst_fitness = first.standardized_fitness;
        tree_t* tree = &first.tree;
        for (auto& ind : pop.get_individuals())
        {
            if (ind.standardized_fitness > worst_fitness)
            {
                worst_fitness = ind.standardized_fitness;
                tree = &ind.tree;
            }
        }
        return *tree;
    }
    
    tree_t& select_random_t::select(gp_program& program, population_t& pop, population_stats&)
    {
        return pop.get_individuals()[program.get_random().get_size_t(0ul, pop.get_individuals().size())].tree;
    }
    
    tree_t& select_tournament_t::select(gp_program& program, population_t& pop, population_stats&)
    {
        
        auto& first = pop.get_individuals()[program.get_random().get_size_t(0ul, pop.get_individuals().size())];
        individual* ind = &first;
        double best_guy = first.standardized_fitness;
        for (blt::size_t i = 0; i < selection_size - 1; i++)
        {
            auto& sel = pop.get_individuals()[program.get_random().get_size_t(0ul, pop.get_individuals().size())];
            if (sel.standardized_fitness < best_guy)
            {
                best_guy = sel.standardized_fitness;
                ind = &sel;
            }
        }
        
        return ind->tree;
    }
    
    tree_t& select_fitness_proportionate_t::select(gp_program& program, population_t& pop, population_stats&)
    {
        auto choice = program.get_random().get_double();
        for (const auto& ind : blt::enumerate(pop))
        {
            if (ind.first == pop.get_individuals().size()-1)
                return ind.second.tree;
            if (choice > ind.second.probability && pop.get_individuals()[ind.first+1].probability < choice)
                return ind.second.tree;
        }
        BLT_WARN("Unable to find individual with fitness proportionate. This should not be a possible code path!");
        return pop.get_individuals()[0].tree;
        //BLT_ABORT("Unable to find individual");
    }
    
    void select_fitness_proportionate_t::pre_process(gp_program&, population_t& pop, population_stats& stats)
    {
        double sum_of_prob = 0;
        for (auto& ind : pop)
        {
            ind.probability = sum_of_prob + (ind.standardized_fitness / stats.overall_fitness);
            sum_of_prob += ind.probability;
        }
    }
}