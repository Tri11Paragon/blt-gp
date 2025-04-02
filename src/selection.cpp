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
    void select_best_t::pre_process(gp_program&, population_t&)
    {
        // std::sort(pop.begin(), pop.end(), [](const auto& a, const auto& b)
        // {
        //     return a.fitness.adjusted_fitness > b.fitness.adjusted_fitness;
        // });
        index = 0;
    }

    const tree_t& select_best_t::select(gp_program&, const population_t& pop)
    {
        const auto size = pop.get_individuals().size();
        return pop.get_individuals()[index.fetch_add(1, std::memory_order_relaxed) % size].tree;
    }

    void select_worst_t::pre_process(gp_program&, population_t&)
    {
        // std::sort(pop.begin(), pop.end(), [](const auto& a, const auto& b)
        // {
        //     return a.fitness.adjusted_fitness < b.fitness.adjusted_fitness;
        // });
        index = 0;
    }

    const tree_t& select_worst_t::select(gp_program&, const population_t& pop)
    {
        const auto size = pop.get_individuals().size();
        return pop.get_individuals()[(size - 1) - (index.fetch_add(1, std::memory_order_relaxed) % size)].tree;
    }

    const tree_t& select_random_t::select(gp_program& program, const population_t& pop)
    {
        return pop.get_individuals()[program.get_random().get_size_t(0ul, pop.get_individuals().size())].tree;
    }

    const tree_t& select_tournament_t::select(gp_program& program, const population_t& pop)
    {
        thread_local hashset_t<u64> already_selected;
        already_selected.clear();
        auto& i_ref = pop.get_individuals();

        u64 best = program.get_random().get_u64(0, pop.get_individuals().size());
        for (size_t i = 0; i < std::min(selection_size, pop.get_individuals().size()); i++)
        {
            u64 sel_point;
            do
            {
                sel_point = program.get_random().get_u64(0ul, pop.get_individuals().size());;
            }
            while (already_selected.contains(sel_point));
            already_selected.insert(sel_point);
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
            }
            else
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
