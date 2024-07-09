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
#include <blt/gp/program.h>

namespace blt::gp
{
    
    // default static references for mutation, crossover, and initializer
    // this is largely to not break the tests :3
    // it's also to allow for quick setup of a gp program if you don't care how crossover or mutation is handled
    static mutation_t s_mutator;
    static crossover_t s_crossover;
    static ramped_half_initializer_t s_init;
    
    gp_program::config_t::config_t(size_t populationSize, size_t initialMinTreeSize, size_t initialMaxTreeSize):
            population_size(populationSize), initial_min_tree_size(initialMinTreeSize), initial_max_tree_size(initialMaxTreeSize), mutator(s_mutator),
            crossover(s_crossover), pop_initializer(s_init)
    {}
    
    gp_program::config_t::config_t(size_t populationSize, size_t initialMinTreeSize, size_t initialMaxTreeSize,
                                   const std::reference_wrapper<mutation_t>& mutator, const std::reference_wrapper<crossover_t>& crossover,
                                   const std::reference_wrapper<population_initializer_t>& popInitializer):
            population_size(populationSize), initial_min_tree_size(initialMinTreeSize), initial_max_tree_size(initialMaxTreeSize), mutator(mutator),
            crossover(crossover), pop_initializer(popInitializer)
    {}
    
    gp_program::config_t::config_t(size_t populationSize, size_t initialMinTreeSize, size_t initialMaxTreeSize,
                                   const std::reference_wrapper<population_initializer_t>& popInitializer):
            population_size(populationSize), initial_min_tree_size(initialMinTreeSize), initial_max_tree_size(initialMaxTreeSize),
            mutator(s_mutator), crossover(s_crossover), pop_initializer(popInitializer)
    {}
    
    gp_program::config_t::config_t(): mutator(s_mutator), crossover(s_crossover), pop_initializer(s_init)
    {
    
    }
    
    gp_program::config_t::config_t(const std::reference_wrapper<population_initializer_t>& popInitializer):
            mutator(s_mutator), crossover(s_crossover), pop_initializer(popInitializer)
    {
    
    }
}