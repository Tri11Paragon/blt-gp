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
#include <blt/logging/logging.h>

using namespace blt::gp;

std::atomic_uint64_t normal_construct = 0;
std::atomic_uint64_t ephemeral_construct = 0;
std::atomic_uint64_t normal_drop = 0;
std::atomic_uint64_t ephemeral_drop = 0;
std::atomic_uint64_t max_allocated = 0;

struct drop_type
{
    float* m_value;
    bool ephemeral = false;

    drop_type() : m_value(new float(0))
    {
        ++normal_construct;
    }

    explicit drop_type(const float silly) : m_value(new float(silly))
    {
        ++normal_construct;
    }

    explicit drop_type(const float silly, bool) : m_value(new float(silly)), ephemeral(true)
    {
        // BLT_TRACE("Constructor with value %f", silly);
        ++ephemeral_construct;
    }

    [[nodiscard]] float value() const
    {
        return *m_value;
    }

    void drop() const
    {
        if (ephemeral)
        {
            std::cout << ("Ephemeral drop") << std::endl;
            ++ephemeral_drop;
        }
        else
            ++normal_drop;
        delete m_value;
    }

    friend std::ostream& operator<<(std::ostream& os, const drop_type& dt)
    {
        os << dt.m_value;
        return os;
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

operation_t add{[](const drop_type a, const drop_type b) { return drop_type{a.value() + b.value()}; }, "add"};
operation_t addf{[](const float a, const float b) { return a + b; }, "addf"};
operation_t sub([](const drop_type a, const drop_type b) { return drop_type{a.value() - b.value()}; }, "sub");
operation_t subf([](const float a, const float b) { return a - b; }, "subf");
operation_t mul([](const drop_type a, const drop_type b) { return drop_type{a.value() * b.value()}; }, "mul");
operation_t mulf([](const float a, const float b) { return a * b; }, "mulf");
operation_t pro_div([](const drop_type a, const drop_type b) { return drop_type{b.value() == 0.0f ? 0.0f : a.value() / b.value()}; }, "div");
operation_t pro_divf([](const float a, const float b) { return b == 0.0f ? 0.0f : a / b; }, "divf");
operation_t op_sin([](const drop_type a) { return drop_type{std::sin(a.value())}; }, "sin");
operation_t op_sinf([](const float a) { return std::sin(a); }, "sinf");
operation_t op_cos([](const drop_type a) { return drop_type{std::cos(a.value())}; }, "cos");
operation_t op_cosf([](const float a) { return std::cos(a); }, "cosf");
operation_t op_exp([](const drop_type a) { return drop_type{std::exp(a.value())}; }, "exp");
operation_t op_expf([](const float a) { return std::exp(a); }, "expf");
operation_t op_log([](const drop_type a) { return drop_type{a.value() <= 0.0f ? 0.0f : std::log(a.value())}; }, "log");
operation_t op_logf([](const float a) { return a <= 0.0f ? 0.0f : std::log(a); }, "logf");
operation_t op_tof([](const drop_type a) { return a.value(); }, "to_f");
operation_t op_todrop([](const float a) { return drop_type{a}; }, "to_drop");
operation_t op_mixed_input([](const drop_type a, const float f)
{
    return a.value() + f;
}, "mixed_input");
auto lit = operation_t([]()
{
    return drop_type{regression.get_program().get_random().get_float(-1.0f, 1.0f), true};
}, "lit").set_ephemeral();

auto litf = operation_t([]()
{
    return regression.get_program().get_random().get_float(-1.0f, 1.0f);
}, "litf").set_ephemeral();

operation_t op_x([](const context& context)
{
    return drop_type{context.x};
}, "x");

operation_t op_xf([](const context& context)
{
    return context.x;
}, "xf");

bool fitness_function(const tree_t& current_tree, fitness_t& fitness, size_t)
{
    if (normal_construct - normal_drop > max_allocated)
        max_allocated = normal_construct - normal_drop;
    constexpr static double value_cutoff = 1.e15;
    for (auto& fitness_case : regression.get_training_cases())
    {
        BLT_GP_UPDATE_CONTEXT(fitness_case);
        auto val = current_tree.get_evaluation_ref<drop_type>(fitness_case);
        const auto diff = std::abs(fitness_case.y - val.get().value());
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
    return static_cast<size_t>(fitness.hits) == regression.get_training_cases().size();
}

int main()
{
    operator_builder<context> builder{};
    builder.build(add, sub, mul, pro_div, op_sin, op_cos, op_exp, op_log, op_mixed_input, lit, op_x, addf, subf, mulf, pro_divf, op_sinf, op_cosf,
                  op_expf, op_logf,
                  litf, op_xf, op_tof, op_todrop);
    regression.get_program().set_operations(builder.grab());

    auto& program = regression.get_program();
    static auto sel = select_tournament_t{};
    program.generate_initial_population(program.get_typesystem().get_type<drop_type>().id());
    program.setup_generational_evaluation(fitness_function, sel, sel, sel);
    while (!program.should_terminate())
    {
        BLT_TRACE("---------------\\{Begin Generation {}}---------------", program.get_current_generation());
        BLT_TRACE("Creating next generation");
        program.create_next_generation();
        BLT_TRACE("Move to next generation");
        program.next_generation();
        BLT_TRACE("Evaluate Fitness");
        program.evaluate_fitness();
    }

    // program.get_best_individuals<1>()[0].get().tree.print(program, std::cout, true, true);

    regression.get_program().get_current_pop().clear();
    regression.get_program().next_generation();
    regression.get_program().get_current_pop().clear();

    BLT_TRACE("Created {} times", normal_construct.load());
    BLT_TRACE("Dropped {} times", normal_drop.load());
    BLT_TRACE("Ephemeral created {} times", ephemeral_construct.load());
    BLT_TRACE("Ephemeral dropped {} times", ephemeral_drop.load());
    BLT_TRACE("Max allocated {} times", max_allocated.load());

    if (ephemeral_construct.load() != ephemeral_drop.load())
        BLT_ABORT("Failed to drop all ephemeral objects");

    if (normal_construct.load() != normal_drop.load())
        BLT_ABORT("Failed to drop all objects during runtime");
}
