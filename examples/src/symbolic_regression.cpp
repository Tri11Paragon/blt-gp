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
#include "../symbolic_regression.h"

static const unsigned long SEED = std::random_device()();

blt::gp::prog_config_t config = blt::gp::prog_config_t()
        .set_initial_min_tree_size(2)
        .set_initial_max_tree_size(6)
        .set_elite_count(2)
        .set_crossover_chance(0.9)
        .set_mutation_chance(0.0)
        .set_reproduction_chance(0.25)
        .set_max_generations(50)
        .set_pop_size(500)
        .set_thread_count(0);

int main()
{
    blt::gp::example::symbolic_regression_t regression{config, SEED};
    regression.execute();

    BLT_TRACE("%lf vs %lf", blt::gp::parent_fitness.load(std::memory_order_relaxed), blt::gp::child_fitness.load(std::memory_order_relaxed));
    
    return 0;
}