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

// you can either use a straight numeric seed, or provide a function which produces a u64 output which will initialize the thread local random number generators.
static const auto SEED_FUNC = [] { return std::random_device()(); };

blt::gp::grow_generator_t grow_generator;
blt::gp::full_generator_t full_generator;

blt::gp::ramped_half_initializer_t ramped_half_initializer;
blt::gp::full_initializer_t full_initializer;

int main()
{
    // config options can be chained together to form compound statements.
    blt::gp::prog_config_t config = blt::gp::prog_config_t()
            .set_initial_min_tree_size(2)
            .set_initial_max_tree_size(6)
            .set_elite_count(2)
            .set_crossover_chance(0.9)
            .set_mutation_chance(0.1)
            .set_reproduction_chance(0.25)
            .set_max_generations(50)
            .set_pop_size(500)
            .set_thread_count(16);

    // example on how you can change the mutation config
    blt::gp::mutation_t::config_t mut_config{};
    mut_config.generator = full_generator;
    mut_config.replacement_min_depth = 2;
    mut_config.replacement_max_depth = 6;

    blt::gp::advanced_mutation_t mut_adv{mut_config};
    blt::gp::mutation_t mut{mut_config};

    // you can choose to set any type of system used by the GP. Mutation, Crossover, and Initializers
    // (config options changed do not affect others, so you can programmatically change them at runtime)
    config.set_initializer(ramped_half_initializer);
    config.set_mutation(mut_adv);

    // the config is copied into the gp_system so changing the config will not change the runtime of the program.
    blt::gp::example::symbolic_regression_t regression{SEED_FUNC, config};

    regression.execute();

    return 0;
}

bool blt::gp::example::symbolic_regression_t::fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t) const
{
    constexpr static double value_cutoff = 1.e15;
    for (auto& fitness_case : training_cases)
    {
        const auto diff = std::abs(fitness_case.y - current_tree.get_evaluation_value<float>(fitness_case));
        if (diff < value_cutoff)
        {
            fitness.raw_fitness += diff;
            if (diff <= 0.01)
                fitness.hits++;
        }
        else
            fitness.raw_fitness += value_cutoff;
    }
    fitness.standardized_fitness = fitness.raw_fitness;
    fitness.adjusted_fitness = (1.0 / (1.0 + fitness.standardized_fitness));
    return static_cast<size_t>(fitness.hits) == training_cases.size();
}

void blt::gp::example::symbolic_regression_t::setup_operations()
{
    BLT_DEBUG("Setup Types and Operators");
    static operation_t add{[](const float a, const float b) { return a + b; }, "add"};
    static operation_t sub([](const float a, const float b) { return a - b; }, "sub");
    static operation_t mul([](const float a, const float b) { return a * b; }, "mul");
    static operation_t pro_div([](const float a, const float b) { return b == 0.0f ? 0.0f : a / b; }, "div");
    static operation_t op_sin([](const float a) { return std::sin(a); }, "sin");
    static operation_t op_cos([](const float a) { return std::cos(a); }, "cos");
    static operation_t op_exp([](const float a) { return std::exp(a); }, "exp");
    static operation_t op_log([](const float a) { return a == 0.0f ? 0.0f : std::log(a); }, "log");
    static auto lit = operation_t([this]()
    {
        return program.get_random().get_float(-1.0f, 1.0f);
    }, "lit").set_ephemeral();

    static operation_t op_x([](const context& context)
    {
        return context.x;
    }, "x");

    operator_builder<context> builder{};
    builder.build(add, sub, mul, pro_div, op_sin, op_cos, op_exp, op_log, lit, op_x);
    program.set_operations(builder.grab());
}
