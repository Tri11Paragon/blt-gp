/*
 *  <Short Description>
 *  Copyright (C) 2025  Brett Terpstra
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
#include "../examples/symbolic_regression.h"
#include <blt/gp/program.h>
#include <blt/std/logging.h>

using namespace blt::gp;

struct drop_type
{
    float silly_type;

    void drop() const
    {
        BLT_TRACE("Wow silly type of value %f was dropped!", silly_type);
    }
};

struct context
{
    float x, y;
};

prog_config_t config = prog_config_t()
                       .set_initial_min_tree_size(2)
                       .set_initial_max_tree_size(6)
                       .set_elite_count(2)
                       .set_crossover_chance(0.8)
                       .set_mutation_chance(0.1)
                       .set_reproduction_chance(0.1)
                       .set_max_generations(50)
                       .set_pop_size(500)
                       .set_thread_count(0);


example::symbolic_regression_t regression{691ul, config};

operation_t add{[](const float a, const float b) { return a + b; }, "add"};
operation_t sub([](const float a, const float b) { return a - b; }, "sub");
operation_t mul([](const float a, const float b) { return a * b; }, "mul");
operation_t pro_div([](const float a, const float b) { return b == 0.0f ? 0.0f : a / b; }, "div");
operation_t op_sin([](const float a) { return std::sin(a); }, "sin");
operation_t op_cos([](const float a) { return std::cos(a); }, "cos");
operation_t op_exp([](const float a) { return std::exp(a); }, "exp");
operation_t op_log([](const float a) { return a == 0.0f ? 0.0f : std::log(a); }, "log");
operation_t op_conv([](const drop_type d) { return d.silly_type; }, "conv");
auto lit = operation_t([]()
{
    return drop_type{regression.get_program().get_random().get_float(-1.0f, 1.0f)};
}, "lit").set_ephemeral();

operation_t op_x([](const context& context)
{
    return context.x;
}, "x");

int main()
{
    operator_builder<context> builder{};
    builder.build(add, sub, mul, pro_div, op_sin, op_cos, op_exp, op_log, lit, op_x);
    regression.get_program().set_operations(builder.grab());

    regression.generate_initial_population();
    auto& program = regression.get_program();
    while (!program.should_terminate())
    {
        BLT_TRACE("Creating next generation");
        program.create_next_generation();
        BLT_TRACE("Move to next generation");
        program.next_generation();
        BLT_TRACE("Evaluate Fitness");
        program.evaluate_fitness();
    }
}
