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
    
    tree_t& select_best_t::select(gp_program&, population_t& pop, population_stats& stats)
    {
        auto& first = pop.getIndividuals()[0];
        double best_fitness = first.adjusted_fitness;
        tree_t* tree = &first.tree;
        for (auto& ind : pop.getIndividuals())
        {
            if (ind.adjusted_fitness < best_fitness)
            {
                best_fitness = ind.adjusted_fitness;
                tree = &ind.tree;
            }
        }
        return *tree;
    }
    
    tree_t& select_worst_t::select(gp_program&, population_t& pop, population_stats& stats)
    {
        auto& first = pop.getIndividuals()[0];
        double worst_fitness = first.adjusted_fitness;
        tree_t* tree = &first.tree;
        for (auto& ind : pop.getIndividuals())
        {
            if (ind.adjusted_fitness > worst_fitness)
            {
                worst_fitness = ind.adjusted_fitness;
                tree = &ind.tree;
            }
        }
        return *tree;
    }
    
    tree_t& select_random_t::select(gp_program& program, population_t& pop, population_stats& stats)
    {
        // TODO: use a more generic randomness solution.
        std::uniform_int_distribution dist(0ul, pop.getIndividuals().size());
        return pop.getIndividuals()[dist(program.get_random())].tree;
    }
    
    tree_t& select_tournament_t::select(gp_program& program, population_t& pop, population_stats& stats)
    {
        std::uniform_int_distribution dist(0ul, pop.getIndividuals().size());
        
        auto& first = pop.getIndividuals()[dist(program.get_random())];
        individual* ind = &first;
        double best_guy = first.adjusted_fitness;
        for (blt::size_t i = 0; i < selection_size - 1; i++)
        {
            auto& sel = pop.getIndividuals()[dist(program.get_random())];
            if (sel.adjusted_fitness < best_guy)
            {
                best_guy = sel.adjusted_fitness;
                ind = &sel;
            }
        }
        
        return ind->tree;
    }
    
    // https://www.google.com/search?client=firefox-b-d&sca_esv=71668abf73626b35&sca_upv=1&biw=1916&bih=940&sxsrf=ADLYWIJehgPtkALJDoTgHCiO4GNeQppSeA:1720490607140&q=roulette+wheel+selection+pseudocode&uds=ADvngMgiq8uozSRb4WPAa_ESRaBJz-G_Xhk1OLU3QFjqc3o31P4ECuIkKJxHd-cR3WUe9U7VQGpI6NRaMgYiWTMd4wNofAAaNq6X4eHYpN8cR9HmTfTw0KgYC6gI4dgu-s-5mXivdsv4QxrkVAL7yMoXacJngsiMBg&udm=2&sa=X&ved=2ahUKEwig7Oj77piHAxU3D1kFHS1lAIsQxKsJegQIDBAB&ictx=0#vhid=6iCOymnPvtyy-M&vssid=mosaic
    tree_t& select_fitness_proportionate_t::select(gp_program& program, population_t& pop, population_stats& stats)
    {
    
    }
}